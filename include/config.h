/*
The cap on the number of recorded entries to keep. This allows you to prevent 
the database from becoming bloated.

When the cap is reached, entries are deleted one-by-one, starting with the 
oldest.
*/
#define MAX_LOG_ENTRIES                                  200

/*
The default maximum number of entries to display when calling 'mattime show'.
*/
#define ENTRIES_TO_SHOW                                   10

/*
The user's standard directory used to store app data. Path is relative to 
$HOME.

A `mattime/` directory will be created within this to store the database and 
log file.

By default this location is `~/.local/share/`, so e.g. the database would be 
stored at `~/.local/share/mattime/mattime.db`.
*/
#define APP_DATA_PATH                        ".local/share/"

/*
If this is set to 1, the above (APP_DATA_PATH) will be overridden by the 
$XDG_DATA_HOME environment variable if it is set.
*/
#define XDG_DATA_HOME_OVERRIDE                             1

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