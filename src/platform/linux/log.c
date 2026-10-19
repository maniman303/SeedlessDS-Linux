#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

int __android_log_print(int priority, const char *tag, const char *format, ...)
{
    char message[1024];
    va_list args;
    (void)priority;
    va_start(args, format);
    int result = vsnprintf(message, sizeof message, format, args);
    va_end(args);
    if (result < 0) return result;
    message[sizeof message - 1] = 0;
    fprintf(stderr, "%s: %s\n", tag ? tag : "SeedlessDS", message);
    return result;
}
