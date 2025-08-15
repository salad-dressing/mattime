#include <sqlite3.h>

int              add(int argc, char* argv[], sqlite3* db);
int            total(int argc, sqlite3* db);
int             show(int argc, sqlite3* db);
int            force(int argc, char* argv[], sqlite3* db);
int             undo(int argc, sqlite3* db);
int            reset(int argc, sqlite3* db);