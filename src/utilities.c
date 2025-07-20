#include <stdio.h>
#include <stdarg.h>

#define USR_ERROR_FD stderr
#define USR_DEBUG_FD stdout

/*
Prints a message to the user (stdout for debug/info, stderr for error).

 * ARGUMENTS: 
    error: set to 1 for error, set to 0 for error
    format: format string to print

 * RETURN VALUE:
    None.

 * NOTE:
    This function should not be called directly by the developer. Instead, 
    the macros USR_DEBUG and USR_ERROR should be used.
*/
void usr_print(unsigned int error, const char* format, ...)
{
    
    va_list args;
    va_start(args, format);
    
    if (error)
    {
        /* Print extra details about the error. */

        fprintf(USR_ERROR_FD, "[!] ");
        vfprintf(USR_ERROR_FD, format, args);
        fprintf(USR_ERROR_FD, "Please check the log at ~/.local/state/mattime/"
                        "debug_log for more details.\n");
        fprintf(USR_ERROR_FD, "\n");
    }
    else
    {
        fprintf(USR_DEBUG_FD, "[*] ");
        vfprintf(USR_DEBUG_FD, format, args);
        fprintf(USR_DEBUG_FD, "\n");
    }

    va_end(args);

    return;
}


/*
Prints a message to the log file.

 * ARGUMENTS: 
    error: set to 1 for error, set to 0 for error
    format: format string to print

 * RETURN VALUE:
    None.

 * NOTE:
    This function should not be called directly by the developer. Instead, 
    the macros LOG_DEBUG and LOG_ERROR should be used.
*/
void log_print(unsigned int error, const char* file, unsigned int line, 
               const char* func, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    
    if (error)
    {
        /* Print extra details about the error. */

        fprintf(stderr, "[%s:%d] %s(): ", file, line, func);
    }
    else
    {
        fprintf(stderr, "[*] ");
    }
    vfprintf(stderr, format, args);

    fprintf(stderr, "\n");
    va_end(args);

    return;
}

