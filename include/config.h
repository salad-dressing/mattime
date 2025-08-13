/*
The cap on the number of recorded entries to keep. This allows you to prevent 
the database from becoming bloated.

When the cap is reached, entries are deleted one-by-one, starting with the 
oldest.
*/
#define MAX_LOG_ENTRIES                                   20

/*
The default maximum number of entries to display when calling 'mattime show'.
*/
#define ENTRIES_TO_SHOW                                   10

/*
Path to write debug/error logs to. Relative to $HOME.

If this is changed, you will have to manually move the file to the new 
destination, otherwise a new file will be created.
*/
#define LOGFILE_PATH      ".local/share/mattime/mattime.log"

/*
Path to the mattime database. Relative to $HOME.

If this is changed, you will have to manually move the file to the new 
destination, otherwise a new file will be created.
*/
#define DATABASE_PATH      ".local/share/mattime/mattime.db"

/*
Set to 1 to have the most recent entries at the top, e.g.:

| Total Hrs | Added Hrs | Date            | Time  |
| 128.50    | 1.50      | Wed 06 Aug 2025 | 10:46 |
| 127.00    | 2.00      | Wed 06 Aug 2025 | 09:53 |
| 125.00    | 0.00      | Fri 01 Aug 2025 | 11:59 |

or set to 0 to have the most recent entries at the bottom, e.g.:

| 125.00    | 0.00      | Fri 01 Aug 2025 | 11:59 |
| 127.00    | 2.00      | Wed 06 Aug 2025 | 09:53 |
| 128.50    | 1.50      | Wed 06 Aug 2025 | 10:46 |
*/
#define SHOW_MOST_RECENT_AT_TOP                            0