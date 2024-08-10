#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sqlite3.h>
#include <math.h>

#define MAX_LOG_ENTRIES 20
#define ENTRIES_TO_SHOW 10

int                                       info();
int                                       help();
int                                       date();

int            initialiseDatabase(sqlite3* logs);

int   add(int argc, char* argv[], sqlite3* logs);
int total(int argc, char* argv[], sqlite3* logs);
int  show(int argc, char* argv[], sqlite3* logs);
int force(int argc, char* argv[], sqlite3* logs);
int  undo(int argc, char* argv[], sqlite3* logs);
int reset(int argc, char* argv[], sqlite3* logs);



int main(int argc, char* argv[]) {

    sqlite3* logs;

    if (sqlite3_open("logs.db", &logs) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(logs));
        sqlite3_close(logs);
        return 1;
    }

    if (argc == 1) {
        info();
        sqlite3_close(logs);
        return 0;
    }

    initialiseDatabase(logs);

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        help();
    }
    else if (strcmp(argv[1], "date") == 0  || strcmp(argv[1], "-d") == 0) {
        date();
    }
    else if (strcmp(argv[1], "add") == 0 || strcmp(argv[1], "-a") == 0) {
        add(argc, argv, logs);
    }
    else if (strcmp(argv[1], "total") == 0 || strcmp(argv[1], "-t") == 0) {
        total(argc, argv, logs);
    }
    else if (strcmp(argv[1], "show") == 0 || strcmp(argv[1], "-s") == 0) {
        show(argc, argv, logs);
    }
    else if (strcmp(argv[1], "force") == 0 || strcmp(argv[1], "-f") == 0) {
        force(argc, argv, logs);
    }
    else if (strcmp(argv[1], "undo") == 0 || strcmp(argv[1], "-u") == 0) {
        undo(argc, argv, logs);
    }
    else if (strcmp(argv[1], "reset") == 0 || strcmp(argv[1], "-r") == 0) {
        reset(argc, argv, logs);
    }
    else {
        fprintf(stdout, "Option not recognised.\n");
        info();
    }

    sqlite3_close(logs);
    return 0;
}

static int floatCallback(void* data, int argc, char** argv, char** azColName) {
    if (argc == 1 && argv[0]) {
        *(float*)data = atof(argv[0]); // Convert the result to float and store it in data
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


int info() {
    // Prints basic information

    fprintf(stdout, "mattime: progress-logging utility for martial art enthusiasts.\n\
Try 'mattime help' for more information.\n");
    return 0;
}

int help() {
    fprintf(stdout, "mattime: progress-logging utility for martial art enthusiasts.\n\
Usage: mattime [OPTION] ...\n\n\
Options:\n\
  -h, help              Displays the help page\n\
  -d, date              Displays current time and date\n\
  -a, add               Record an entry with the specified number of hours\n\
  -t, total             Displays the total number of hours and last updated\n\
  -s, show              Show the last 10 entries\n\
  -f, force             Force-set the total hours to the specified value\n\
  -u, undo              Remove the last entry\n\
  -r, reset             Reset the entire log\n\n\
Examples:\n\
  mattime add 10        Adds 10 hours to the total\n\
  mattime force 50      Total hours is now set to 50\n\n\
For feedback or issues, please report to developer: saladdressing@mail.com\n\
");
    return 0;
}

int date() {
    // Shows the current time and date

    time_t rawDatetime = time(NULL);
    struct tm* Datetime = localtime(&rawDatetime);
    char datetimeBuffer[80];

    strftime(datetimeBuffer, 80, "%H:%M | %d/%m/%Y", Datetime);
    fprintf(stderr, "Current time & date: %s\n", datetimeBuffer);
    return 0;
}

int initialiseDatabase(sqlite3* logs) {
    // Ensures a table is created
    
    char* createTableCommand = "CREATE TABLE IF NOT EXISTS Sessions(TotalHours FLOAT, HoursAdded FLOAT, Date TINYTEXT, Time TINYTEXT);";
    char* errorMessage;
    if (sqlite3_exec(logs, createTableCommand, 0, 0, &errorMessage) != SQLITE_OK) {
        fprintf(stderr, "Creating table failed!\nSQL error: %s\n", errorMessage);
        return 1;
    }
    return 0;
}

int add(int argc, char* argv[], sqlite3* logs) {
    // Adds <float> number of hours to the total, adding an entry to the log and popping the oldest one off

    if (argc == 2) {
        fprintf(stderr, "mattime add: missing argument\nTry 'mattime --help' for more information.\n");
        return 1;
    } 
    else if (argc == 3) {
        float userInputHours = atof(argv[2]);
        if (userInputHours != 0) {

            // Build date and time strings
            time_t rawDatetime = time(NULL);
            struct tm* Datetime = localtime(&rawDatetime);
            char dateBuffer[80];
            char timeBuffer[80];
            strftime(dateBuffer, 80, "%d/%m/%Y", Datetime);
            strftime(timeBuffer, 80, "%H:%M", Datetime);

            // Update total hours
            float previousTotalHours = 0;
            char* queryTotalCommand = "SELECT TotalHours FROM Sessions WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";
            sqlite3_exec(logs, queryTotalCommand, floatCallback, &previousTotalHours, 0);
            float newTotalHours = previousTotalHours + userInputHours;

            // Build and execute SQL command
            char addEntryCommand[500];
            char* errorMessage1; int returnCode1;

            snprintf(addEntryCommand, 500,
                    "INSERT INTO Sessions (TotalHours, HoursAdded, Date, Time) VALUES (%0.4f, %0.4f, \"%s\", \"%s\");", 
                    newTotalHours, userInputHours, dateBuffer, timeBuffer);
            if (sqlite3_exec(logs, addEntryCommand, 0, 0, &errorMessage1) == SQLITE_OK) {
                fprintf(stdout, "Successfully logged %s hours.\n", argv[2]);
            } else {
                fprintf(stderr, "Adding entry failed!\nSQL error: %s\n", errorMessage1);
                return 1;
            }

            // easter eggs
            if (fmodf(previousTotalHours, 100) > fmodf(newTotalHours, 100)) {
                fprintf(stdout, "\nCongratulations!! You passed a multiple of 100!!\n");
            }
            else if (fmodf(previousTotalHours, 50) > fmodf(newTotalHours, 50)) {
                fprintf(stdout, "\nCongratulations! You passed a multiple of 50!\n");
            }

            // Maintain max. number of log entries
            int numberOfRows = 0;
            char* errorMessage2; char* errorMessage3;
            char* countRowsCommand = "SELECT COUNT(*) FROM Sessions;";

            if (sqlite3_exec(logs, countRowsCommand, intCallback, &numberOfRows, &errorMessage2) != SQLITE_OK) {
                fprintf(stderr, "Counting rows failed!\nSQL error: %s\n", errorMessage2);
                return 1;
            }
            
            if (numberOfRows > MAX_LOG_ENTRIES) {
                char* deleteRowCommand = "DELETE FROM Sessions WHERE ROWID = (SELECT MIN(ROWID) FROM Sessions);";
                if (sqlite3_exec(logs, deleteRowCommand, 0, 0, &errorMessage3) != SQLITE_OK) {
                    fprintf(stderr, "Deleting rows failed!\nSQL error: %s\n", errorMessage3);
                    return 1;
                }
            }

            return 0;
        }
    }
    else {
        fprintf(stderr, "mattime add: too many arguments\nTry 'mattime --help' for more information.\n");
        return 1;
    }
}

int total(int argc, char* argv[], sqlite3* logs) {
    // Displays current number of hours, with last updated
    
    float totalNumberOfHours = 0; char* correspondingDateTime;
    char* queryTotalHoursCommand = "SELECT TotalHours FROM Sessions WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";
    char* queryCorrespondingDateTimeCommand = "SELECT Time, Date FROM Sessions WHERE ROWID = (SELECT MAX(ROWID) FROM Sessions);";

    char* errorMessage1; char* errorMessage2;
    int returnCode1; int returnCode2;

    returnCode1 = sqlite3_exec(logs, queryTotalHoursCommand, floatCallback, &totalNumberOfHours, &errorMessage1);
    returnCode2 = sqlite3_exec(logs, queryCorrespondingDateTimeCommand, stringCallback, &correspondingDateTime, &errorMessage2);

    if (totalNumberOfHours == 0) {
        // empty table
        fprintf(stdout, "Total number of hours: 0.\nNo entries added.\nTry 'mattime add' to add an entry, or 'mattime help' for more options.\n");
        return 0;
    }

    if (returnCode1 == SQLITE_OK && returnCode2 == SQLITE_OK) {
        fprintf(stdout, "Total number of hours: %0.1f\nLast updated: %s\n", totalNumberOfHours, correspondingDateTime);
        return 0;
    } else if (returnCode1 != SQLITE_OK && returnCode2 == SQLITE_OK) {
        fprintf(stderr, "Querying total hours failed!\nSQL error: %s\n", errorMessage1);
        return 1;
    } else if (returnCode1 == SQLITE_OK && returnCode2 != SQLITE_OK) {
        fprintf(stderr, "Querying corresponding date & time failed!\nSQL error: %s\n", errorMessage2);
        return 1;
    } else {
        fprintf(stderr, "Many things failed!\nSQL error(s): \n%s\n%s\n", errorMessage1, errorMessage2);
        return 1;
    }
}

int show(int argc, char* argv[], sqlite3* logs) {
    // Displays last 10 entries

    if (argc == 2) {
        char printEntriesCommand[500];
        snprintf(printEntriesCommand, 500, 
                "SELECT * FROM Sessions WHERE ROWID > ((SELECT MAX(ROWID) FROM Sessions) - %d);", ENTRIES_TO_SHOW);

        fprintf(stdout, "Recently logged entries:\n(Note: when the total hours is force-set, the added hours is listed as 0.)\n\n");
        fprintf(stdout, "| Total Hrs     | Added Hrs     | Date       | Time  |\n");
        sqlite3_exec(logs, printEntriesCommand, stringCallback, 0, 0);
        fprintf(stdout, "\n");
    }
    else {
        fprintf(stderr, "mattime show: too many arguments\nTry 'mattime --help' for more information.\n");
        return 1;
    }

    return 0;
}

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