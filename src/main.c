#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>

#include "options.h"
#include "utilities.h"

void info();
void help();
int  init_db(sqlite3* logs);

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
    "  -r, reset             Reset the entire log\n\n"
    "Examples:\n"
    "  mattime add 10        Adds 10 hours to the total\n"
    "  mattime force 50      Total hours is now set to 50\n\n"
    "For feedback or issues, please report to developer: " 
    "saladdressing@mail.com\n";

/*
Ensure that a Sessions table is created, if not already.

 * ARGUMENTS:
    db: ptr to a sqlite3 object

 * RETURN VALUE:
    Returns 0 upon success, 1 otherwise.
*/
int init_db(sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    const char* cmd = "CREATE TABLE IF NOT EXISTS "
                      "Sessions(TotalHours FLOAT, HoursAdded FLOAT, "
                      "Time BIGINT);";
    char* error_msg = NULL;

    rc = sqlite3_exec(db, cmd, 0, 0, &error_msg);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("Creating table failed.\n SQL error: %s\n", error_msg);
        USR_ERROR("Failed to initialise the database.\n");

        ret = 1;
        goto cleanup;
    }
    
    /* Success. */

    ret = 0;

cleanup:
    if (error_msg)
    {
        sqlite3_free(error_msg);
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
    char* home = NULL;
    char* db_path = NULL;
    sqlite3* db = NULL;

    /*
    Initialise the database.
    */

    home = getenv("HOME");
    if (!home)
    {
        USR_DEBUG("Environment variable $HOME is not set.\n" 
              "Please set this so that logs can be created in the appropriate "
              "location.\n");

        goto cleanup;
    }

    size_t buf_len = strlen(home) + 1 + strlen(DATABASE);
    
    /* Dynamically allocate due to the goto jump before this.
    If it was static memory, the jump would bypass the initialisation of 
    db_path on the stack. */
    db_path = calloc(1, buf_len + 1);

    rc = snprintf(db_path, buf_len + 1, "%s/%s", home, DATABASE);
    if (rc < (int)buf_len)
    {
        /* Catch both errors and early truncation. */

        LOG_ERROR("call to snprintf with parameters:\n" 
                  " * db_path (buffer)\n" 
                  " * buf_len: %zu\n" 
                  " * home: %s\n",
                  " * DATABASE: %s\n",
                  "returned: %d (expected: %d)\n",
                  buf_len, home, DATABASE, rc, buf_len);
        USR_ERROR("An error occurred creating the path to the log file.\n");

        goto cleanup;
    }
    db_path[buf_len] = '\0';

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("cannot open database.\n"
                  "Error: %s\n", sqlite3_errmsg(db));
        USR_ERROR("An error occurred opening the database.\n");

        goto cleanup;
    }

    /*
    Process user arguments and initialise the database.
    */

    if (argc == 1)
    {
        info();
        goto cleanup;
    }

    rc = init_db(db);
    if (rc)
    {
        goto cleanup;
    }

    /*
    Handle the request.
    */

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
#if 0
    else if (!strcmp(argv[1], "force") || !strcmp(argv[1], "-f"))
    {
        ret = force(argc, argv, db);
    }

    else if (!strcmp(argv[1], "undo") || !strcmp(argv[1], "-u"))
    {
        ret = undo(argc, argv, db);
    }

    else if (!strcmp(argv[1], "reset") || !strcmp(argv[1], "-r"))
    {
        ret = reset(argc, argv, db);
    }
#endif
    else
    {
        USR_DEBUG("mattime: option not recognised.\n");
        info();
    }

cleanup:

    if (db_path)
    {
        free(db_path);
    }
    
    if (db)
    {
        sqlite3_close(db);
    }

    return ret;
}