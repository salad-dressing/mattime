#include <bits/types/FILE.h>

/*
Set in main.c: init_log().
*/
extern FILE* g_log_fp;

/*
Prints a debug/info message to the log file.
*/
#define LOG_DEBUG(fmt, ...) log_print(0, __FILE__, __LINE__, __func__, fmt, \
##__VA_ARGS__)

/*
Prints an error message to the log file.
*/
#define LOG_ERROR(fmt, ...) log_print(1, __FILE__, __LINE__, __func__, fmt, \
##__VA_ARGS__)

/*
Prints a debug/info message to stdout.
*/
#define USR_DEBUG(fmt, ...) usr_print(0, fmt, ##__VA_ARGS__)

/*
Prints an error message to stderr.
*/
#define USR_ERROR(fmt, ...) usr_print(1, fmt, ##__VA_ARGS__)

void usr_print(unsigned int error, const char* format, ...);

void log_print(unsigned int error, const char* file, unsigned int line, 
               const char* func, const char* format, ...);