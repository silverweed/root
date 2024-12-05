#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

typedef enum {
  Log_None,
  Log_Fatal,
  Log_Err,
  Log_Warn,
  Log_Info,
  Log_Debug,
  Log_Verbose,
  Log_Very_Verbose,

  Log_COUNT,
  Log_MAX = Log_COUNT - 1
} Log_Level;

inline Log_Level g_loglv = Log_Info;

inline
const char *g_loglv_name[] = {
  "", "FATAL", "ERR", "WARN", "INFO", "DEBUG", "VERBOSE", "VERBOS2"
};

inline
void log_set_lv(Log_Level lv)
{
  g_loglv = Fae_Min(lv, Log_MAX);
}

inline
Log_Level log_get_lv()
{
  return g_loglv;
}

inline
void fae_log(Log_Level lv, const char *tag, const char *fmt, ...)
{
  thread_local char g_logbuf[16 * 1024];

  assert(lv > Log_None);
  
  if (lv > g_loglv)
    return;
  
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_logbuf, countof(g_logbuf), fmt, args);
  u32 logidx = Fae_Min(Log_MAX, lv);
  fprintf(stderr, "[%s] %s: %s\n", tag, g_loglv_name[logidx], g_logbuf);
  va_end(args);
}

#define FATAL_TAG(tag, fmt, ...)        fae_log(Log_Fatal,        tag, fmt, ## __VA_ARGS__)
#define ERR_TAG(tag, fmt, ...)          fae_log(Log_Err,          tag, fmt, ## __VA_ARGS__)
#define WARN_TAG(tag, fmt, ...)         fae_log(Log_Warn,         tag, fmt, ## __VA_ARGS__)
#define INFO_TAG(tag, fmt, ...)         fae_log(Log_Info,         tag, fmt, ## __VA_ARGS__)
#define DEBUG_TAG(tag, fmt, ...)        fae_log(Log_Debug,        tag, fmt, ## __VA_ARGS__)
#define VERBOSE_TAG(tag, fmt, ...)      fae_log(Log_Verbose,      tag, fmt, ## __VA_ARGS__)
#define VERY_VERBOSE_TAG(tag, fmt, ...) fae_log(Log_Very_Verbose, tag, fmt, ## __VA_ARGS__)

#define FATAL(fmt, ...)        FATAL_TAG("Generic", fmt, ## __VA_ARGS__)
#define ERR(fmt, ...)          ERR_TAG("Generic", fmt, ## __VA_ARGS__)
#define WARN(fmt, ...)         WARN_TAG("Generic", fmt, ## __VA_ARGS__)
#define INFO(fmt, ...)         INFO_TAG("Generic", fmt, ## __VA_ARGS__)
#define DEBUG(fmt, ...)        DEBUG_TAG("Generic", fmt, ## __VA_ARGS__)
#define VERBOSE(fmt, ...)      VERBOSE_TAG("Generic", fmt, ## __VA_ARGS__)
#define VERY_VERBOSE(fmt, ...) VERY_VERBOSE_TAG("Generic", fmt, ## __VA_ARGS__)
