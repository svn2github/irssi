#ifndef __PRINTTEXT_H
#define __PRINTTEXT_H

#include "windows.h"

void printformat_module(const char *module, void *server, const char *target, int level, int formatnum, ...);
void printformat_module_window(const char *module, WINDOW_REC *window, int level, int formatnum, ...);

void printformat_module_args(const char *module, void *server, const char *target, int level, int formatnum, va_list va);
void printformat_module_window_args(const char *module, WINDOW_REC *window, int level, int formatnum, va_list va);

void printtext(void *server, const char *target, int level, const char *text, ...);
void printtext_window(WINDOW_REC *window, int level, const char *text, ...);
void printtext_multiline(void *server, const char *target, int level, const char *format, const char *text);
void printbeep(void);

void printtext_init(void);
void printtext_deinit(void);

/* printformat(...) = printformat_format(MODULE_NAME, ...)

   Could this be any harder? :) With GNU C compiler and C99 compilers,
   use #define. With others use either inline functions if they are
   supported or static functions if they are not..
 */
#if defined (__GNUC__) && !defined (__STRICT_ANSI__)
/* GCC */
#  define printformat(server, target, level, formatnum...) \
	printformat_module(MODULE_NAME, server, target, level, ##formatnum)
#  define printformat_window(window, level, formatnum...) \
	printformat_module_window(MODULE_NAME, window, level, ##formatnum)
#elif defined (_ISOC99_SOURCE)
/* C99 */
#  define printformat(server, target, level, formatnum, ...) \
	printformat_module(MODULE_NAME, server, target, level, formatnum, __VA_ARGS__)
#  define printformat_window(window, level, formatnum, ...) \
	printformat_module_window(MODULE_NAME, window, level, formatnum, __VA_ARGS__)
#else
/* inline/static */
#ifdef G_CAN_INLINE
G_INLINE_FUNC
#else
static
#endif
void printformat(void *server, const char *target, int level, int formatnum, ...)
{
	va_list va;

	va_start(va, formatnum);
	printformat_module_args(MODULE_NAME, server, target, level, formatnum, va);
	va_end(va);
}

#ifdef G_CAN_INLINE
G_INLINE_FUNC
#else
static
#endif
void printformat_window(WINDOW_REC *window, int level, int formatnum, ...)
{
	va_list va;

	va_start(va, formatnum);
	printformat_module_window_args(MODULE_NAME, window, level, formatnum, va);
	va_end(va);
}
#endif

#endif
