#include <sqlite3.h>

int              add(int argc, char* argv[], sqlite3* logs);
int            total(int argc, sqlite3* logs);
int             show(int argc, sqlite3* logs);
int            force(int argc, char* argv[], sqlite3* logs);
int             undo(int argc, sqlite3* logs);
int            reset(int argc, char* argv[], sqlite3* logs);