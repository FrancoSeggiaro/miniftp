#pragma once
#include <syslog.h>

void log_init(const char *app_name);
void log_write(int priority, const char *format, ...);
void log_close();