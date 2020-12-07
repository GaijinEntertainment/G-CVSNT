#pragma once
#include <stdio.h>
#include <stdarg.h>
#include "../blob_push_log.h"

void blob_logmessage(int log, const char *fmt,...)
{
  if (log < LOG_WARNING)
    return;
  va_list va;
  va_start(va, fmt);
  vfprintf(log >= LOG_ERROR ? stderr : stdout, fmt, va);
  va_end(va);
  fprintf(log >= LOG_ERROR ? stderr : stdout, "\n");
}

