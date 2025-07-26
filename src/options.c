//#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>
#include <errno.h>

#include "utilities.h"
#include "options.h"

/*
Safely assume for all options that argc > 1.
*/
#if 0
static int floatCallback(void* data, int argc, char** argv, char** azColName) {
    if (argc == 1 && argv[0]) {
        *(float*)data = strtof(argv[0], NULL); // Convert the result to float and store it in data
    }
    return 0;
}

static int intCallback(void *data, int argc, char **argv, char **azColName) {
    if (argc == 1 && argv[0]) {
        *(int*)data = atoi(argv[0]); // Convert the result to int and store it in data
    }
    return 0;
}

static int stringCallback(void *data, int argc, char **argv, char **azColName) {

    // for add()
    if (argc == 2) {
        char** result = (char **)data;
        char* returnString = strcat(strdup(argv[0]), " | ");
        *result = strcat(returnString, strdup(argv[1]));
    }

    // for show() and undo()
    else if (argc == 4) {
        char* returnString = (char *) malloc(200);
        returnString[0] = '\0'; //initialise

        strcat(returnString, "|   ");
        strcat(returnString, strdup(argv[0]));
        strcat(returnString, "    |    ");
        strcat(returnString, strdup(argv[1]));
        strcat(returnString, "    | ");
        strcat(returnString, strdup(argv[2]));
        strcat(returnString, " | ");
        strcat(returnString, strdup(argv[3]));
        strcat(returnString, " |");
        
        fprintf(stdout, "| %-14s| %-14s| %-11s| %-6s|\n", argv[0],argv[1],argv[2],argv[3]);
    }
    

    return 0;
}

#endif

/*
Adds the specified number of hours as an entry.

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
    unsigned int num_of_rows = 0;
    time_t sec = 0;
    float added_hrs = 0.0;
    float prev_total_hrs = 0.0;
    float new_total_hrs = 0.0;
    char* error_msg = NULL;
    char* cmd_tmp = NULL;
    char* add_entry_cmd = NULL;
    const char* query_total_cmd = NULL;
    const char* del_entry_cmd = NULL;
    const char* congrats_msg = NULL;
    const char* query_count_cmd = NULL;
    sqlite3_stmt *add_stmt = NULL;
    sqlite3_stmt *count_stmt = NULL;

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

    errno = 0;
    added_hrs = strtof(argv[2], NULL);
    if (errno)
    {
        LOG_ERROR("call to strtof with:\n"
            " * nptr: %s\n" 
            "failed with errno %d (%s)\n",
            argv[2], errno, strerror(errno));
        USR_ERROR("An error occurred parsing the argument 'hours'.\n");

        goto cleanup;
    }

    if (added_hrs == 0.0)
    {
        /* strtof returns 0 when no conversion was performed. 
        This (although unlikely) could be intentional, so don't exit. */

        LOG_DEBUG("call to strtof with:\n"
            " * nptr: %s\n" 
            "returned %f.\n",
            argv[2], added_hrs);

        USR_DEBUG("Read 'hours' as 0.\n" 
            "If this wasn't intended, an error occurred parsing user " 
            "input.\n");
    }

    if (added_hrs < 0.0)
    {
        LOG_ERROR("attempted entry with negative hours: %d\n", added_hrs);
        USR_ERROR("mattime add: hours must be positive.\n");

        goto cleanup;
    }

    /* Before adding an entry, enforce the maximum number of log entries to 
    keep in the database. If it would exceed the limit, remove the oldest 
    entry first. */

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

    if (num_of_rows > MAX_LOG_ENTRIES)
    {
        /* Delete the oldest entry from the table to make room. */

        del_entry_cmd = "DELETE FROM Sessions "
            "WHERE ROWID = (SELECT MIN(ROWID) FROM Sessions);";

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
            congrats_msg = "Congratulations, you passed a multiple of 100!\n";
        }
        else if (((int)prev_total_hrs % 50) > ((int)new_total_hrs % 50))
        {
            milestone = 50;
            congrats_msg = "Congratulations, you passed a multiple of 50!\n";
        }
    }

    /* Construct the command to add the new entry to the table, and execute 
    it. Print any congratulations. */

    cmd_tmp = "INSERT INTO Sessions (TotalHours, HoursAdded, Time) "
        "VALUES (%0.2f, %0.2f, %lu);";

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
            " * new_total_hrs: %0.2f\n"
            " * added_hrs: %0.2f\n"
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
        LOG_DEBUG("call to sqlite3_exec with:\n"
            " * cmd: %s\n" 
            "returned error message: %s\n",
            add_entry_cmd, error_msg);

        USR_DEBUG("An error occurred adding an entry to the database.\n");

        goto cleanup;
    }

    if (congrats_msg)
    {
        LOG_DEBUG("passed milestone:\n" 
            " * multiple of: %u\n"
            " * previous total: %0.2f\n"
            " * new total: %0.2f\n",
            milestone, prev_total_hrs, new_total_hrs);

        USR_DEBUG(congrats_msg);
    }

    /* Success. */

    LOG_DEBUG("added entry:\n" 
        " * TotalHours: %0.2f\n"
        " * HoursAdded: %0.2f\n"
        " * Time: %lu\n",
        new_total_hrs, added_hrs, (unsigned long)sec);

    USR_DEBUG("Successfully added %0.2f hours.\n", added_hrs);

    ret = 0;

cleanup:
    if (add_stmt)
    {
        sqlite3_finalize(add_stmt);
    }
    if (count_stmt)
    {
        sqlite3_finalize(count_stmt);
    }
    if (error_msg)
    {
        sqlite3_free(error_msg);
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
    if (rc != SQLITE_ROW)
    {
        /* Should have retrieved a single row. */

        LOG_ERROR("sqlite3_step failed with error message: %s\n",
            sqlite3_errmsg(db));
        USR_ERROR("An error occurred fetching from the database.\n");

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

    fprintf(stdout, "Total hours: %0.1f\n", total_hrs);
    fprintf(stdout, "Last updated: %s %s\n", date_str, time_str);
    
    /* Success. */

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

    if (rc != SQLITE_DONE)
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

#if 0
int force(int argc, char* argv[], sqlite3* logs) {
    // Force-sets the number of hours to a value (with double checking)

    if (argc == 2) {
        fprintf(stderr, "mattime set: missing argument\nTry 'mattime --help' for more information.\n");
        return 1;
    }

    else if (argc == 3) {
        float userInputTotalHours = atof(argv[2]);
        if (userInputTotalHours > 0) {

            // Build date and time strings
            time_t rawDatetime = time(NULL);
            struct tm* Datetime = localtime(&rawDatetime);
            char dateBuffer[80];
            char timeBuffer[80];
            strftime(dateBuffer, 80, "%d/%m/%Y", Datetime);
            strftime(timeBuffer, 80, "%H:%M", Datetime);

            // Build and execute SQL command
            char addEntryCommand[500];
            char* errorMessage; int returnCode;
            snprintf(addEntryCommand, 500,
                    "INSERT INTO Sessions (TotalHours, HoursAdded, Date, Time) VALUES (%0.4f, %d, \"%s\", \"%s\");", 
                    userInputTotalHours, 0, dateBuffer, timeBuffer);
    
            fprintf(stdout, "Confirm action: force total hours to %0.1f? (Type y/n)\n", userInputTotalHours);
            char response = '0'; scanf("%c", &response);
            if (response == 'y') {
                returnCode = sqlite3_exec(logs, addEntryCommand, 0, 0, &errorMessage);
                if (returnCode == SQLITE_OK) {
                    fprintf(stdout, "Successfully set total hours to %0.1f.\n", userInputTotalHours);
                } else {
                    fprintf(stderr, "Setting hours failed!\nSQL error: %s\n", errorMessage);
                    return 1;
                }
            }
            else if (response == 'n') {
                fprintf(stdout, "Action aborted.\n");
                return 1;
            }
            else {
                fprintf(stderr, "Response not recognised.\n");
                return 1;
            }

            // Maintain a maximum number of log entries
            int numberOfRows = 0;
            char* countRowsCommand = "SELECT COUNT(*) FROM Sessions;";
            sqlite3_exec(logs, countRowsCommand, intCallback, &numberOfRows, 0);
            if (numberOfRows > MAX_LOG_ENTRIES) {
                char* deleteRowCommand = "DELETE FROM Sessions WHERE ROWID = (SELECT MIN(ROWID) FROM Sessions);";
                sqlite3_exec(logs, deleteRowCommand, 0, 0, 0);
            } 
        }

        else {
            fprintf(stderr, "Invalid hours requested.\n");
            return 1;
        }
    }

    else {
        fprintf(stderr, "mattime set: too many arguments\nTry 'mattime --help' for more information.\n");
        return 1;
    }

    return 0;

}

int undo(int argc, char* argv[], sqlite3* logs) {
    // Removes latest entry

    char* showLatestEntryCommand = "SELECT * FROM Sessions WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";
    char* deleteLatestEntryCommand = "DELETE FROM Sessions WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";

    if (argc == 2) {
        char* errorMessage1; char* errorMessage2; int returnCode1; int returnCode2;
        
        fprintf(stdout, "Confirm action: remove latest entry:\n\n");
        fprintf(stdout, "| Total Hrs     | Added Hrs     | Date       | Time  |\n");
        returnCode1 = sqlite3_exec(logs, showLatestEntryCommand, stringCallback, 0, &errorMessage1);
        if (returnCode1 != SQLITE_OK) {
            fprintf(stderr, "Requesting latest entry failed!\nSQL error: %s\n", errorMessage1);
            return 1;
        }
        fprintf(stdout, "(Type y/n)\n");
        char response = '0'; scanf("%c", &response);

        if (response == 'y') {
            returnCode2 = sqlite3_exec(logs, deleteLatestEntryCommand, 0, 0, &errorMessage2);
            if (returnCode2 == SQLITE_OK) {
                fprintf(stdout, "Successfully removed latest entry.\n");
            } else {
                fprintf(stderr, "Deleting latest entry failed!\nSQL error: %s\n", errorMessage2);
                return 1;
            }
        }
        else if (response == 'n') {
            fprintf(stdout, "Action aborted.\n");
            return 1;
        }
        else {
            fprintf(stderr, "Response not recognised.\n");
            return 1;
        }
        
    }

    else {
        fprintf(stderr, "mattime undo: too many arguments\nTry 'mattime --help' for more information.\n");
        return 1;
    }

    return 0;
}


int reset(int argc, char* argv[], sqlite3* logs) {
    // Clears all entries from the table

    char* resetCommand = "DELETE FROM Sessions;";
    char* errorMessage;

    fprintf(stdout, "Confirm action: delete all entries and reset?\n(Type y/n)\n");
    char response = '0'; scanf("%c", &response);

    if (response == 'y') {
        getchar();
        fprintf(stdout, "\nWARNING: This action is permanent and cannot be undone. Continue to delete all entries?\n(Type y/n)\n");
        char confirmation = '0'; scanf("%c", &confirmation);
        if (confirmation == 'y') {
            if (sqlite3_exec(logs, resetCommand, 0, 0, &errorMessage) == SQLITE_OK) {
                fprintf(stdout, "Successfully deleted all entries.\n");
                return 0;
            }
            else {
                fprintf(stderr, "Deleting all entries failed!\nSQL error: %s\n", errorMessage);
                return 1;
            }
        } else if (confirmation == 'n') {
            fprintf(stdout, "Action aborted.\n");
            return 1;
        } else {
            fprintf(stderr, "Response not recognised. Aborting...\n");
            return 1;
        }
    }

    else if (response == 'n') {
        fprintf(stdout, "Action aborted.\n");
        return 1;
    }

    else {
        fprintf(stderr, "Response not recognised. Aborting...\n");
        return 1;
    }
}
#endif