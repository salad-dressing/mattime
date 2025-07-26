#include <sqlite3.h>

#define MAX_LOG_ENTRIES                                   20
#define ENTRIES_TO_SHOW                                   10
#define DATABASE           ".local/share/mattime/mattime.db"
#define DATETIME_BUFSIZE                                1024
#define SHOW_MOST_RECENT_AT_TOP                            0

int              add(int argc, char* argv[], sqlite3* logs);
int            total(int argc, sqlite3* logs);
int             show(int argc, sqlite3* logs);
int            force(int argc, char* argv[], sqlite3* logs);
int             undo(int argc, char* argv[], sqlite3* logs);
int            reset(int argc, char* argv[], sqlite3* logs);