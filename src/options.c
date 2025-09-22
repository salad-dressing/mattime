//#include <math.h>
#include <bits/types/time_t.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>
#include <errno.h>

#include "config.h"
#include "utilities.h"
#include "options.h"

/*
Safely assume for all options that argc > 1.
*/


/*
Parses the user input (number of hours) as a float, copying into the memory 
of 'value'.

 * ARGUMENTS:
    input: user input
    value: pointer to memory of resulting float

 * RETURN VALUE:
    Returns 0 upon success, 1 otherwise.
*/
static int parse_hours(const char* input, float* value)
{
    int ret = 1;

    float input_flt = 0.0;

    errno = 0;
    input_flt = strtof(input, NULL);
    if (errno)
    {
        LOG_ERROR("call to strtof with:\n"
            " * nptr: %s\n" 
            "failed with errno %d (%s)\n",
            input, errno, strerror(errno));
        USR_ERROR("An error occurred parsing the argument 'hours'.\n");

        goto exit;
    }

    if (input_flt == 0.0)
    {
        /* strtof returns 0 when no conversion was performed. 
        This (although unlikely) could be intentional, so don't exit. */

        LOG_DEBUG("call to strtof with:\n"
            " * nptr: %s\n" 
            "returned %f.\n",
            input, input_flt);

        USR_DEBUG("Read 'hours' as 0.\n" 
            "If this wasn't intentional, an error occurred parsing the user " 
            "input.\n");
    }

    /* Success. */

    *value = input_flt;
    ret = 0;

exit:
    return ret;
}

/*
Enforces a limit of N entries in the database, deleting oldest entries first.
Pre-empts the imminent addition of another row, leaving a maximum of N-1 
entries in the database.

 * ARGUMENTS:
    N: maximum number of entries allowed
    db: pointer to the SQL database object
*/
static int maintain_max_entries(unsigned int N, sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    int len = 0;
    int bytes_written = 0;
    unsigned int num_of_rows = 0;
    char* error_msg = NULL;
    char* del_entry_cmd = NULL;
    const char* query_count_cmd = NULL;
    const char* del_entry_cmd_tmp = NULL;
    sqlite3_stmt *count_stmt = NULL;

    query_count_cmd = "SELECT COUNT(*) FROM Sessions;";

    rc = sqlite3_prepare_v2(db, query_count_cmd, -1, &count_stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_prepare_v2 with:\n"
            " * cmd: %s\n" 
            "failed with error message: %s\n",
            query_count_cmd, sqlite3_errmsg(db));
        USR_ERROR("An error occurred preparing the database query.\n");

        goto cleanup;
    }

    rc = sqlite3_step(count_stmt);
    if (rc != SQLITE_ROW)
    {
        /* Should have retrieved a single row. */

        LOG_ERROR("sqlite3_step failed with error message: %s\n",
            sqlite3_errmsg(db));
        USR_ERROR("An error occurred fetching from the database.\n");

        goto cleanup;
    }

    num_of_rows = (unsigned int)sqlite3_column_int(count_stmt, 0);

    if (num_of_rows >= N)
    {
        /* Delete the oldest entry from the table to make room. */

        del_entry_cmd_tmp = "DELETE FROM Sessions WHERE ROWID IN "
            "(SELECT ROWID FROM Sessions ORDER BY ROWID ASC LIMIT %u);";

        len = snprintf(NULL, 0, del_entry_cmd_tmp, N);
        del_entry_cmd = calloc(1, len + 1);
        bytes_written = snprintf(del_entry_cmd, len + 1, del_entry_cmd_tmp, N);
        if (bytes_written < len)
        {
            /* Catch both errors and early truncation. */

            LOG_ERROR("call to snprintf with parameters:\n" 
                " * del_entry_cmd (buffer)\n" 
                " * len + 1: %d\n" 
                " * del_entry_cmd_tmp: %s\n"
                " * N: %u\n"
                "returned: %d (expected: %d)\n",
                len + 1, del_entry_cmd_tmp, N, bytes_written, len);
            USR_ERROR("An error occurred constructing the query to the " 
                "database.\n");

            goto cleanup;
        }

        rc = sqlite3_exec(db, del_entry_cmd, 0, 0, &error_msg);
        if (rc != SQLITE_OK)
        {
            LOG_DEBUG("call to sqlite3_exec with:\n"
                " * cmd: %s\n" 
                "returned error message: %s\n",
                del_entry_cmd, error_msg);

            USR_DEBUG("An error occurred removing an entry from the "
                "database.\n");

            goto cleanup;
        }
    }

    /* Success. */

    ret = 0;

cleanup:

    if (del_entry_cmd)
    {
        free(del_entry_cmd);
    }
    if (error_msg)
    {
        sqlite3_free(error_msg);
    }
    if (count_stmt)
    {
        sqlite3_finalize(count_stmt);
    }
    
    return ret;
}

/*
Receive [y/n] from the user on whether to continue or not.
If happy to proceed, function returns 0 printing nothing.
If unhappy or an error occurs, return 1 and prints an appropriate message.

 * ARGUMENTS:
    new_hrs: the number of hours to confirm as new total

 * RETURN VALUE:
    Returns 0 on 'yes', 1 otherwise.
*/
static int usr_confirm()
{
    int ret = 1;

    // Safe size for \n, \0 etc.
    #define BUF_LEN 10

    char input[BUF_LEN] = {0};
    
    if (!fgets(input, sizeof(input), stdin))
    {
        LOG_ERROR("fgets returned NULL; buffer: %s\n", input);
        USR_ERROR("An error occurred reading user input.");

        goto exit;
    }
    input[BUF_LEN - 1] = '\0'; // Double-check before possibly printing

    if ((input[0] == 'y' || input[0] == 'Y') && input[1] == '\n')
    {
        /* Happy to proceed. */

        ret = 0;
    }
    else if ((input[0] == 'n' || input[0] == 'N') && input[1] == '\n')
    {
        USR_DEBUG("Aborting...\n");
    }
    else
    {
        LOG_ERROR("user input detected as: %s\n"
            "Not recognised as one of [y/Y/n/N].\n",
            input);
        USR_ERROR("Unrecognised input. Aborting...\n");
    }

exit:
    return ret;
}

/*
Adds an entry to the database.

 * ARGUMENTS:
    argc
    argv
    db: pointer to the SQL database object

 * RETURN VALUE:
    Returns 0 if success, 1 otherwise.
*/
int add(int argc, char* argv[], sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    int len = 0;
    int bytes_written = 0;
    unsigned int milestone = 0;
    time_t sec = 0;
    float added_hrs = 0.0;
    float prev_total_hrs = 0.0;
    float new_total_hrs = 0.0;
    char* error_msg = NULL;
    char* cmd_tmp = NULL;
    char* add_entry_cmd = NULL;
    const char* query_total_cmd = NULL;
    const char* congrats_msg = NULL;
    sqlite3_stmt *add_stmt = NULL;
    struct tm* tm_info = NULL;
    char date_str[17]; // e.g. 'Sun 20 Jul 2025' + null
    char time_str[6];  // e.g. '11:24' + null

    if (argc == 2)
    {
        USR_DEBUG("mattime add: missing argument 'hours'.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    if (argc > 3)
    {
        USR_DEBUG("mattime add: too many arguments.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    /* Parse the user input. */

    rc = parse_hours(argv[2], &added_hrs);
    if (rc)
    {
        goto cleanup;
    }

    if (added_hrs < 0.0)
    {
        LOG_ERROR("attempted entry with negative hours: %d\n", added_hrs);
        USR_ERROR("mattime add: hours must be positive.\n");

        goto cleanup;
    }

    /* Before adding an entry, enforce the maximum number of log entries to 
    keep in the database. */

    rc = maintain_max_entries(MAX_LOG_ENTRIES, db);
    if (rc)
    {
        goto cleanup;
    }

    /* Now prepare the components for adding an entry. */
    /* Retrieve the current seconds since the Epoch. */

    errno = 0;
    sec = time(NULL);
    if (errno || sec < 0) // Both should be True upon an error
    {
        LOG_ERROR("call to time(NULL) failed with errno: %d (%s)\n",
            errno, strerror(errno));

        USR_ERROR("An error occurred fetching the current time.\n");

        goto cleanup;
    }

    /* Fetch the previous total hours. */

    query_total_cmd = "SELECT TotalHours FROM Sessions "
        "WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";

    rc = sqlite3_prepare_v2(db, query_total_cmd, -1, &add_stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_prepare_v2 with:\n"
            " * cmd: %s\n" 
            "failed with error message: %s\n",
            query_total_cmd, sqlite3_errmsg(db));
        USR_ERROR("An error occurred preparing the database query.\n");

        goto cleanup;
    }

    rc = sqlite3_step(add_stmt);
    if (rc == SQLITE_DONE)
    {
        /* Zero results i.e. an empty table. 
        Since this is the first row to input, the new total hours is just 
        the number added. */

        new_total_hrs = added_hrs;
    }
    else if (rc != SQLITE_ROW)
    {
        LOG_ERROR("sqlite3_step failed with error message: %s\n",
            sqlite3_errmsg(db));
        USR_ERROR("An error occurred fetching from the database.\n");

        goto cleanup;
    }
    else
    {
        /* Retrieved the previous total successfully. */

        prev_total_hrs = sqlite3_column_double(add_stmt, 0);
        
        new_total_hrs = prev_total_hrs + added_hrs;

        /* Check for having passed a multiple of 50 or 100 to supply 
        congratulations messages. */

        if (((int)prev_total_hrs % 100) > ((int)new_total_hrs % 100))
        {
            milestone = 100;
            congrats_msg = "Congratulations, you passed a multiple of 100!";
        }
        else if (((int)prev_total_hrs % 50) > ((int)new_total_hrs % 50))
        {
            milestone = 50;
            congrats_msg = "Congratulations, you passed a multiple of 50!";
        }
    }

    /* Construct the command to add the new entry to the table, and execute 
    it. Print any congratulations. */

    cmd_tmp = "INSERT INTO Sessions (TotalHours, HoursAdded, Time) "
        "VALUES (%.2f, %.2f, %lu);";

    len = snprintf(NULL, 0, cmd_tmp, 
                            new_total_hrs, 
                            added_hrs, 
                            (unsigned long)sec);
    add_entry_cmd = calloc(1, len + 1);
    bytes_written = snprintf(add_entry_cmd, len + 1, 
                            cmd_tmp, 
                            new_total_hrs, 
                            added_hrs, 
                            (unsigned long)sec);
    if (bytes_written < len)
    {
        /* Catch both errors and early truncation. */

        LOG_ERROR("call to snprintf with parameters:\n" 
            " * cmd (buffer)\n" 
            " * len + 1: %d\n" 
            " * cmd_tmp: %s\n"
            " * new_total_hrs: %.2f\n"
            " * added_hrs: %.2f\n"
            " * sec: %lu\n"
            "returned: %d (expected: %d)\n",
            len + 1, cmd_tmp, new_total_hrs, added_hrs, 
            (unsigned long)sec, bytes_written, len);
        USR_ERROR("An error occurred constructing the query to the " 
            "database.\n");

        goto cleanup;
    }

    rc = sqlite3_exec(db, add_entry_cmd, 0, 0, &error_msg);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_exec with:\n"
            " * cmd: %s\n" 
            "returned error message: %s\n",
            add_entry_cmd, error_msg);

        USR_ERROR("An error occurred adding an entry to the database.\n");

        goto cleanup;
    }

    if (congrats_msg)
    {
        LOG_DEBUG("passed milestone:\n" 
            " * multiple of: %u\n"
            " * previous total: %.2f\n"
            " * new total: %.2f\n",
            milestone, prev_total_hrs, new_total_hrs);

        USR_DEBUG(congrats_msg);
    }

    /* Success. */

    tm_info = localtime(&sec);

    strftime(date_str, sizeof(date_str), "%a %d %b %Y", tm_info);
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    LOG_DEBUG("added entry:\n" 
        " * TotalHours: %.2f\n"
        " * HoursAdded: %.2f\n"
        " * Time: %s, %s\n",
        new_total_hrs, added_hrs, time_str, date_str);

    USR_DEBUG("Successfully added %.2f hours.\n", added_hrs);

    ret = 0;

cleanup:
    if (add_stmt)
    {
        sqlite3_finalize(add_stmt);
    }
    if (error_msg)
    {
        sqlite3_free(error_msg);
    }
    if (add_entry_cmd)
    {
        free(add_entry_cmd);
    }

    return ret;
}

/*
Displays the current total number of hours.

 * ARGUMENTS:
    argc
    db: pointer to the SQL database object

 * RETURN VALUE:
    Returns 0 if success, 1 otherwise.
*/
int total(int argc, sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    sqlite3_stmt *stmt = NULL;
    float total_hrs = 0;
    time_t datetime = 0;

    if (argc != 2)
    {
        USR_DEBUG("mattime total: too many arguments.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    const char* cmd = "SELECT * FROM Sessions "
        "WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions) LIMIT 1;";

    /* Make the query. */

    rc = sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_prepare_v2 with:\n"
            " * cmd: %s\n" 
            "failed with error message: %s\n",
            cmd, sqlite3_errmsg(db));
        USR_ERROR("An error occurred preparing the database query.\n");

        goto cleanup;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ERROR)
    {
        LOG_ERROR("sqlite3_step failed with error message: %s\n",
            sqlite3_errmsg(db));
        USR_ERROR("An error occurred fetching from the database.\n");

        goto cleanup;
    }
    else if (rc != SQLITE_ROW)
    {
        /* Empty database. Safely exit. */

        USR_DEBUG("Total hours: 0.00 (no entries)\n");
        ret = 0;

        goto cleanup;
    }

    /* Present information to the user. The retrieved information is stored 
    within stmt. */

    total_hrs = sqlite3_column_double(stmt, 0);
    datetime = (time_t)sqlite3_column_int(stmt, 2);
    
    struct tm* tm_info = localtime(&datetime);

    char date_str[17]; // e.g. 'Sun 20 Jul 2025' + null
    char time_str[6]; // e.g. '11:24' + null

    strftime(date_str, sizeof(date_str), "%a %d %b %Y", tm_info);
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    /* Success. */

    fprintf(stdout, "Total hours: %.2f\n", total_hrs);
    fprintf(stdout, "Last updated: %s %s\n", date_str, time_str);

    ret = 0;

cleanup:
    if (stmt)
    {
        sqlite3_finalize(stmt);
    }

    return ret;
}

/*
Displays the last N entries to mattime, where N is a user-configurable constant 
(ENTRIES_TO_SHOW).

 * ARGUMENTS:
    argc
    db: pointer to the SQL database object

 * RETURN VALUE:
    Returns 0 if success, 1 otherwise.
*/
int show(int argc, sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    int len = 0;
    int entries = 0;
    int bytes_written = 0;
    char* cmd = NULL;
    const char* cmd_tmp = NULL;
    const char* ordering = NULL;
    sqlite3_stmt *stmt = NULL;

    float total_hrs_arr[ENTRIES_TO_SHOW] = {0.0};
    float added_hrs_arr[ENTRIES_TO_SHOW] = {0.0};
    time_t epoch_sec_arr[ENTRIES_TO_SHOW] = {0};

    if (argc != 2)
    {
        USR_DEBUG("mattime show: too many arguments.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    /* Create query, fetching the length dynamically. */

    cmd_tmp = "SELECT * FROM Sessions WHERE ROWID > "
              "((SELECT MAX(ROWID) FROM Sessions) - %d) %s;";

    if (SHOW_MOST_RECENT_AT_TOP)
    {
        ordering = "ORDER BY ROWID DESC";
    }
    else
    {
        ordering = "";
    }

    len = snprintf(NULL, 0, cmd_tmp, ENTRIES_TO_SHOW, ordering);
    cmd = calloc(1, len + 1);
    bytes_written = snprintf(cmd, len + 1, cmd_tmp, ENTRIES_TO_SHOW, ordering);
    if (bytes_written < len)
    {
        /* Catch both errors and early truncation. */

        LOG_ERROR("call to snprintf with parameters:\n" 
            " * cmd (buffer)\n" 
            " * len + 1: %d\n" 
            " * cmd_tmp: %s\n",
            " * ENTRIES_TO_SHOW: %d\n",
            "returned: %d (expected: %d)\n",
            len + 1, cmd_tmp, ENTRIES_TO_SHOW, bytes_written, len);
        USR_ERROR("An error occurred constructing the query to the " 
            "database.\n");

        goto cleanup;
    }

    /* Make the query. */

    rc = sqlite3_prepare_v2(db, cmd, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_prepare_v2 with:\n"
            " * cmd: %s\n" 
            "failed with error message: %s\n",
            cmd, sqlite3_errmsg(db));
        USR_ERROR("An error occurred preparing the database query.\n");

        goto cleanup;
    }

    while (entries < ENTRIES_TO_SHOW)
    {
        /* Retrieve as many rows of data as possible, up to a maximum of 
        ENTRIES_TO_SHOW. */

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW)
        {
            /* No rows left to process, or error. */
            break;
        }

        total_hrs_arr[entries] = sqlite3_column_double(stmt, 0);
        added_hrs_arr[entries] = sqlite3_column_double(stmt, 1);
        epoch_sec_arr[entries] = (time_t)sqlite3_column_int(stmt, 2);

        entries++;
    }

    if (rc == SQLITE_ERROR)
    {
        LOG_ERROR("sqlite3_step failed with error message: %s\n",
            sqlite3_errmsg(db));
        USR_ERROR("An error occurred fetching from the database.\n");

        goto cleanup;
    }

    /* Present to the user. */

    fprintf(stdout, "Recently logged entries:\n"
        "(Note: when the total hours is force-set, "
        "the added hours is listed as 0.)\n\n");

    fprintf(stdout, "| Total Hrs | Added Hrs | Date            | Time  |\n");
    
    for (int i = 0; i < entries; i++)
    {
        /* Create 'date' and 'time' strings from Epoch seconds. */

        struct tm* tm_info = localtime(&epoch_sec_arr[i]);

        char date_str[17]; // e.g. 'Sun 20 Jul 2025' + null
        char time_str[6]; // e.g. '11:24' + null

        strftime(date_str, sizeof(date_str), "%a %d %b %Y", tm_info);
        strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

        fprintf(stdout,
            "| %-9.2f | %-9.2f | %-15s | %-5s |\n", 
            total_hrs_arr[i],
            added_hrs_arr[i],
            date_str,
            time_str);
    }

    fprintf(stdout, "\n");

    /* Success. */

    ret = 0;

cleanup:
    if (cmd)
    {
        free(cmd);
    }
    if (stmt)
    {
        sqlite3_finalize(stmt);
    }

    return ret;
}

/*
Sets the total number of hours to a given value.

 * ARGUMENTS:
    argc
    argv
    db: pointer to the SQL database object

 * RETURN VALUE:
    Returns 0 if success, 1 otherwise.
*/
int force(int argc, char* argv[], sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    int len = 0;
    int bytes_written = 0;
    time_t sec = 0;
    float new_hrs = 0.0;
    char* cmd = NULL;
    char* error_msg = NULL;
    const char* cmd_tmp = NULL;
    struct tm* tm_info = NULL;
    char date_str[17]; // e.g. 'Sun 20 Jul 2025' + null
    char time_str[6];  // e.g. '11:24' + null

    if (argc == 2)
    {
        USR_DEBUG("mattime force: missing argument 'hours'.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    if (argc > 3)
    {
        USR_DEBUG("mattime force: too many arguments.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    /* Parse the user input. */

    rc = parse_hours(argv[2], &new_hrs);
    if (rc)
    {
        goto cleanup;
    }

    if (new_hrs < 0.0)
    {
        LOG_ERROR("attempted entry with negative hours: %d\n", new_hrs);
        USR_ERROR("mattime force: hours must be positive.\n");

        goto cleanup;
    }

    /* Confirm the action with the user. */

    USR_DEBUG("Confirm action: force total hours to %.2f? [y/n]", new_hrs);
    rc = usr_confirm();
    if (rc)
    {
        goto cleanup;
    }

    /* Before adding an entry, enforce the maximum number of log entries to 
    keep in the database. */

    rc = maintain_max_entries(MAX_LOG_ENTRIES, db);
    if (rc)
    {
        goto cleanup;
    }

    /* Construct and execute the query. */

    errno = 0;
    sec = time(NULL);
    if (errno || sec < 0) // Both should be True upon an error
    {
        LOG_ERROR("call to time(NULL) failed with errno: %d (%s)\n",
            errno, strerror(errno));

        USR_ERROR("An error occurred fetching the current time.\n");

        goto cleanup;
    }

    cmd_tmp = "INSERT INTO Sessions (TotalHours, HoursAdded, Time) "
        "VALUES (%.2f, %.2f, %lu);";

    len = snprintf(NULL, 0, cmd_tmp, 
                            new_hrs, 
                            0.0, 
                            (unsigned long)sec);
    cmd = calloc(1, len + 1);
    bytes_written = snprintf(cmd, len + 1, 
                            cmd_tmp, 
                            new_hrs, 
                            0.0, 
                            (unsigned long)sec);
    if (bytes_written < len)
    {
        /* Catch both errors and early truncation. */

        LOG_ERROR("call to snprintf with parameters:\n" 
            " * cmd (buffer)\n" 
            " * len + 1: %d\n" 
            " * cmd_tmp: %s\n"
            " * new_hrs: %.2f\n"
            " * added_hrs: 0.0\n"
            " * sec: %lu\n"
            "returned: %d (expected: %d)\n",
            len + 1, cmd_tmp, new_hrs, 
            (unsigned long)sec, bytes_written, len);
        USR_ERROR("An error occurred constructing the query to the " 
            "database.\n");

        goto cleanup;
    }

    rc = sqlite3_exec(db, cmd, 0, 0, &error_msg);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_exec with:\n"
            " * cmd: %s\n" 
            "returned error message: %s\n",
            cmd, error_msg);

        USR_ERROR("An error occurred adding an entry to the database.\n");

        goto cleanup;
    }

    /* Success. */

    tm_info = localtime(&sec);

    strftime(date_str, sizeof(date_str), "%a %d %b %Y", tm_info);
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    LOG_DEBUG("forced total:\n" 
        " * TotalHours: %.2f\n"
        " * HoursAdded: 0\n"
        " * Time: %s, %s\n",
        new_hrs, time_str, date_str);

    USR_DEBUG("Success: set total hours to %.2f.\n", new_hrs);
    ret = 0;

cleanup:
    if (cmd)
    {
        free(cmd);
    }
    if (error_msg)
    {
        sqlite3_free(error_msg);
    }
    return ret;
}

/*
Removes the most recent entry in the database.

 * ARGUMENTS:
    argc
    db: pointer to the SQL database object

 * RETURN VALUE:
    Returns 0 if success, 1 otherwise.
*/
int undo(int argc, sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    float total_hrs = 0.0;
    float added_hrs = 0.0;
    time_t time = 0;
    sqlite3_stmt *stmt = NULL;
    struct tm* tm_info = NULL;
    char date_str[17]; // e.g. 'Sun 20 Jul 2025' + null
    char time_str[6];  // e.g. '11:24' + null
    char* error_msg = NULL;
    const char* select_last_cmd = NULL;
    const char* delete_last_cmd = NULL;

    if (argc != 2)
    {
        USR_DEBUG("mattime show: too many arguments.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    /* Obtain the latest entry. */

    select_last_cmd = "SELECT * FROM Sessions "
        "WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions) LIMIT 1;";

    rc = sqlite3_prepare_v2(db, select_last_cmd, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("call to sqlite3_prepare_v2 with:\n"
            " * cmd: %s\n" 
            "failed with error message: %s\n",
            select_last_cmd, sqlite3_errmsg(db));
        USR_ERROR("An error occurred preparing the database query.\n");

        goto cleanup;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ERROR)
    {
        LOG_ERROR("sqlite3_step failed with error message: %s\n",
            sqlite3_errmsg(db));
        USR_ERROR("An error occurred fetching from the database.\n");

        goto cleanup;
    }
    else if (rc != SQLITE_ROW)
    {
        /* No rows left to process. */

        USR_DEBUG("No entries to undo.\n");
        ret = 0;

        goto cleanup;
    }

    total_hrs = sqlite3_column_double(stmt, 0);
    added_hrs = sqlite3_column_double(stmt, 1);
    time = (time_t)sqlite3_column_int(stmt, 2);

    /* Confirm the user wants to proceed. */

    tm_info = localtime(&time);

    strftime(date_str, sizeof(date_str), "%a %d %b %Y", tm_info);
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    USR_DEBUG("Confirm action: remove the following entry? [y/n]\n"
            " * added %.2f hours (total %.2f)\n"
            " * at %s, %s",
            added_hrs, total_hrs, time_str, date_str);
    
    rc = usr_confirm();
    if (rc)
    {
        goto cleanup;
    }

    /* Remove the entry. */

    delete_last_cmd = "DELETE FROM Sessions "
        "WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";

    rc = sqlite3_exec(db, delete_last_cmd, 0, 0, &error_msg);
    if (rc != SQLITE_OK)
    {
        LOG_DEBUG("call to sqlite3_exec with:\n"
            " * cmd: %s\n" 
            "returned error message: %s\n",
            delete_last_cmd, error_msg);

        USR_DEBUG("An error occurred removing the entry from the "
            "database.\n");

        goto cleanup;
    }

    /* Success. */

    LOG_DEBUG("removed entry:\n" 
        " * TotalHours: %.2f\n"
        " * HoursAdded: %.2f\n"
        " * Time: %s, %s\n",
        total_hrs, added_hrs, time_str, date_str);

    USR_DEBUG("Removed entry successfully.");
    ret = 0;

cleanup:

    if (stmt)
    {
        sqlite3_finalize(stmt);
    }
    if (error_msg)
    {
        sqlite3_free(error_msg);
    }

    return ret;
}

/*
Removes all entries from the database.

 * ARGUMENTS:
    argc
    db: pointer to the SQL database object

 * RETURN VALUE:
    Returns 0 if success, 1 otherwise.
*/
int reset(int argc, sqlite3* db)
{
    int ret = 1;

    int rc = 1;
    char* error_msg = NULL;
    const char* reset_cmd = "DELETE FROM Sessions;";
    time_t sec = 0;
    struct tm* tm_info = NULL;
    char date_str[17]; // e.g. 'Sun 20 Jul 2025' + null
    char time_str[6];  // e.g. '11:24' + null
    
    if (argc != 2)
    {
        USR_DEBUG("mattime show: too many arguments.\n"
                  "Try 'mattime --help' for more information.\n");

        goto cleanup;
    }

    /* Confirm action with the user. */
    
    USR_DEBUG("Confirm action: delete all entries from the database? [y/n]\n"
        "WARNING: this action is permanent.");
    
    rc = usr_confirm();
    if (rc)
    {
        goto cleanup;
    }

    /* Delete all entries. */

    rc = sqlite3_exec(db, reset_cmd, 0, 0, &error_msg);
    if (rc != SQLITE_OK)
    {
        LOG_DEBUG("call to sqlite3_exec with:\n"
            " * cmd: %s\n" 
            "returned error message: %s\n",
            reset_cmd, error_msg);

        USR_DEBUG("An error occurred removing the entry from the "
            "database.\n");

        goto cleanup;
    }

    /* Success. */

    sec = time(NULL);
    tm_info = localtime(&sec);

    strftime(date_str, sizeof(date_str), "%a %d %b %Y", tm_info);
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

    LOG_DEBUG("reset database at:\n" 
        " * Time: %s, %s\n",
        time_str, date_str);

    USR_DEBUG("Reset database successfully.");
    ret = 0;

cleanup:

    if (error_msg)
    {
        sqlite3_free(error_msg);
    }

    return ret;
}