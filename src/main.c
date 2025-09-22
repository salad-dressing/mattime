#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "config.h"
#include "options.h"
#include "utilities.h"

void info();
void help();

static const char* info_msg = 
    "mattime: progress-logging utility.\n" 
    "Try 'mattime help' for more information.\n";

static const char* help_msg = 
    "mattime: progress-logging utility.\n"
    "Usage: mattime [OPTION] ...\n\n"
    "Options:\n"
    "  -h, help              Displays the help page\n"
    "  -a, add <hours>       Record an entry with the specified number of " 
    "hours\n"
    "  -t, total             Displays the total number of hours and last " 
    "updated\n"
    "  -s, show              Show the last 10 entries\n"
    "  -f, force <hours>     Force-set the total hours to the specified value\n"
    "  -u, undo              Remove the last entry\n"
    "  -r, reset             Remove all entries\n\n"
    "Examples:\n"
    "  mattime add 10        Adds 10 hours to the total\n"
    "  mattime force 50      Total hours is now set to 50\n\n"
    "For feedback or issues, please report to developer: " 
    "saladdressing@mail.com\n";

/*
Concatenates the directory path with the file.

 * ARGUMENTS:
    dir: path to the directory
    file: filename

 * RETURN VALUE:
    Pointer to the buffer, or NULL on failure.

 * NOTE:
    The buffer is created dynamically. It must be freed by the user before 
    exiting the program.
*/
static char* stitch_path(const char* dir, const char* file)
{
    char* ret = NULL;

    int rc = 1;
    char* buf = NULL;
    size_t len = 0;

    /* Paste the two parts into a new buffer. */

    len = strlen(dir) + 1 + strlen(file);
    buf = calloc(1, len + 1);

    rc = snprintf(buf, len + 1, "%s/%s", dir, file);
    if (rc < (int)len)
    {
        /* Catch both errors and early truncation.

        Don't use USR_ERROR as the boilerplate message asks the user to check 
        the log file, which in this case will not help as it may not yet 
        exist. */

        USR_DEBUG("call to snprintf with parameters:\n" 
                  " * str (buffer)\n" 
                  " * size: %zu\n" 
                  " * dir (arg): %s\n",
                  " * file (arg): %s\n",
                  "returned: %d (expected: %d)\n",
                  len + 1, dir, file, rc, len);

        goto cleanup;
    }

    buf[len] = '\0';

    /* Success. */

    ret = buf;
    buf = NULL;

cleanup:
    if (buf)
    {
        free(buf);
    }

    return ret;
}

/*
Initialise the directory that will hold application data like the logs and 
database.

 * RETURN VALUE:
    Buffer holding the path to the (existing) app data directory, or NULL upon 
    failure.

 * NOTE:
    The buffer is created dynamically. It must be freed by the user before 
    exiting the program.
*/
static char* init_app_data_dir()
{
    char* ret = NULL;

    int rc = 0;
    unsigned int len = 0;
    char* buf = NULL;
    char* xdg_data_home = NULL;
    char* home = NULL;
    char* app_data_dir = NULL;
    char* mattime_dir = NULL;
    struct stat st = {0};

    xdg_data_home = getenv("XDG_DATA_HOME");
    if (XDG_DATA_HOME_OVERRIDE && xdg_data_home)
    {
        /* Success - just allocate this dynamically and return. */

        len = strlen(xdg_data_home) + 1;

        errno = 0;
        buf = malloc(len);
        if (!buf)
        {
            /* No log file available at this point, no choice but to print 
            errno info to the user. */

            USR_DEBUG("malloc returned errno %d (%s)\n", 
                errno, strerror(errno));

            goto cleanup;
        }
        strncpy(buf, xdg_data_home, len + 1);
        buf[len - 1] = '\0'; // Double check

        /* Success. */

        ret = buf;
        buf = NULL;

        goto cleanup;
    }

    /* Else, take the path from APP_DATA_PATH. */

    home = getenv("HOME");
    if (!home)
    {
        /* Similar situation to before regarding log file, we must report 
        directly to the user. */

        USR_DEBUG("The environment variable $HOME is not set.\n"
            "Please set either $HOME or $XDG_DATA_HOME.\n");

        goto cleanup;
    }

    app_data_dir = stitch_path(home, APP_DATA_PATH);
    if (!app_data_dir)
    {
        goto cleanup;
    }

    mattime_dir = stitch_path(app_data_dir, "mattime");
    if (!mattime_dir)
    {
        goto cleanup;
    }
    
    /* Create the directory if it doesn't already exist. */

    rc = stat(mattime_dir, &st);
    if (rc)
    {
        errno = 0;
        rc = mkdir(mattime_dir, 0755); /* In line with other similar dirs. */
        if (rc)
        {
            USR_DEBUG("mkdir failed with errno %d (%s)\n", 
                errno, strerror(errno));

            goto cleanup;
        }
    }

    /* Success. */

    ret = mattime_dir;
    mattime_dir = NULL;

cleanup:
    if (buf)
    {
        free(buf);
    }
    if (app_data_dir)
    {
        free(app_data_dir);
    }
    if (mattime_dir)
    {
        free(mattime_dir);
    }

    return ret;
}

/*
Opens a file descriptor to the log file in the global variable g_log_fp.

 * RETURN VALUE:
    Returns 0 upon success, 1 otherwise.
*/
static int init_log(const char* app_data_dir)
{
    int ret = 1;

    /* g_log_fp declared in utilities.h. */

    /* Creates file if it does not already exist. */

    char* path = stitch_path(app_data_dir, "mattime.log");
    if (!path)
    {
        goto cleanup;
    }

    errno = 0;
    g_log_fp = fopen(path, "a");
    if (!g_log_fp)
    {
        /* Forced to print errno information to the user as there is no other 
        log-like location to print to. 

        Don't use USR_ERROR as the boilerplate message asks the user to check 
        the log file, which in this case will not help. */

        USR_DEBUG("An error occurred initialising the log file: "
            "errno %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    /* Success, g_log_fp is set. */

    ret = 0;

cleanup:
    if (path)
    {
        free(path);
    }

    return ret;
}

/*
Initialise the database and the Sessions table.

 * ARGUMENTS:
    db: ptr to a sqlite3 object

 * RETURN VALUE:
    Returns a pointer to the sqlite3 database object, or NULL upon failure.
*/
static sqlite3* init_db(const char* app_data_dir)
{
    sqlite3* ret = NULL;

    int rc = 1;
    char* db_path = NULL;
    char* error_msg = NULL;
    const char* cmd = NULL;
    sqlite3* db = NULL;

    /* Open the database, creating a new one if it doesn't already exist. */

    db_path = stitch_path(app_data_dir, "mattime.db");
    if (!db_path)
    {
        goto cleanup;
    }

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("cannot open database.\n"
                  "Error: %s\n", sqlite3_errmsg(db));
        USR_ERROR("An error occurred opening the database.\n");

        goto cleanup;
    }

    /* Initialise the Sessions table. */

    cmd = "CREATE TABLE IF NOT EXISTS "
        "Sessions(TotalHours FLOAT, HoursAdded FLOAT, "
        "Time BIGINT);";

    rc = sqlite3_exec(db, cmd, 0, 0, &error_msg);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("Creating table failed.\n SQL error: %s\n", error_msg);
        USR_ERROR("Failed to initialise the database.\n");

        goto cleanup;
    }
    
    /* Success. */

    ret = db;
    db = NULL;

cleanup:
    if (db_path)
    {
        free(db_path);
    }
    if (error_msg)
    {
        sqlite3_free(error_msg);
    }
    if (db)
    {
        sqlite3_close(db);
    }

    return ret;
}

/*
Prints basic info to the user.

 * ARGUMENTS:
    None.

 * RETURN VALUE:
    None.
*/
void info()
{
    fprintf(stdout, "%s", info_msg);
    return;
}

/*
Prints basic help to the user.

 * ARGUMENTS:
    None.

 * RETURN VALUE:
    None.
*/
void help()
{
    fprintf(stdout, "%s", help_msg);
    return;
}

int main(int argc, char* argv[])
{
    int ret = 1;

    int rc = 1;
    char* app_data_dir = NULL;
    sqlite3* db = NULL;

    /*
    Set the app data directory (location to store logs, database etc.)
    */

    app_data_dir = init_app_data_dir();
    if (!app_data_dir)
    {
        goto cleanup;
    }

    /*
    Initialise the log file.
    */

    rc = init_log(app_data_dir);
    if (rc)
    {
        goto cleanup;
    }

    /*
    Initialise the database.
    */

    db = init_db(app_data_dir);
    if (!db)
    {
        goto cleanup;
    }

    /*
    Handle user request.
    */

    if (argc == 1)
    {
        info();
        goto cleanup;
    }

    if (argc == 2 && (!strcmp(argv[1], "help")
                      || !strcmp(argv[1], "-h") 
                      || !strcmp(argv[1], "--help")))
    {
        help();
        ret = 0;
    }
 
    else if (!strcmp(argv[1], "add") || !strcmp(argv[1], "-a"))
    {
        ret = add(argc, argv, db);
    }

    else if (!strcmp(argv[1], "total") || !strcmp(argv[1], "-t"))
    {
        ret = total(argc, db);
    }

    else if (!strcmp(argv[1], "show") || !strcmp(argv[1], "-s"))
    {
        ret = show(argc, db);
    }

    else if (!strcmp(argv[1], "force") || !strcmp(argv[1], "-f"))
    {
        ret = force(argc, argv, db);
    }
     
    else if (!strcmp(argv[1], "undo") || !strcmp(argv[1], "-u"))
    {
        ret = undo(argc, db);
    }

    else if (!strcmp(argv[1], "reset") || !strcmp(argv[1], "-r"))
    {
        ret = reset(argc, db);
    }

    else
    {
        USR_DEBUG("mattime: option not recognised.\n");
        info();
    }

cleanup:

    if (app_data_dir)
    {
        free(app_data_dir);
    }
    if (g_log_fp)
    {
        fclose(g_log_fp);
    }
    if (db)
    {
        sqlite3_close(db);
    }

    return ret;
}