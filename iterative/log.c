#include "log.h"
#include <stdarg.h>

void log_init(const char *app_name) {
    openlog(app_name, LOG_PID, LOG_DAEMON);
}

void log_write(int priority, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}

void log_close() {
    closelog();
}