/* paths */
#undef SYSCONFDIR
#undef HELPDIR
#undef PLUGINSDIR

/* misc.. */
#undef HAVE_IPV6
#undef HAVE_POPT_H
#undef HAVE_SOCKS_H
#undef HAVE_PL_PERL
#undef HAVE_STATIC_PERL
#undef HAVE_GMODULE
#undef HAVE_GC
#undef WANT_BIG5

/* macros/curses checks */
#undef HAS_CURSES
#undef USE_SUNOS_CURSES
#undef USE_BSD_CURSES
#undef USE_SYSV_CURSES
#undef USE_NCURSES
#undef NO_COLOR_CURSES
#undef SCO_FLAVOR

/* our own curses checks */
#undef HAVE_NCURSES_USE_DEFAULT_COLORS
#undef HAVE_CURSES_IDCOK
#undef HAVE_CURSES_RESIZETERM
#undef HAVE_CURSES_WRESIZE

/* terminfo/termcap */
#undef HAVE_TERMINFO

/* nls */
#undef ENABLE_NLS
#undef HAVE_CATGETS
#undef HAVE_GETTEXT
#undef HAVE_LC_MESSAGES
#undef HAVE_STPCPY

/* SSL */
#undef HAVE_OPENSSL

/* If set to 64, enables 64bit off_t for some systems (eg. Linux, Solaris) */
#undef _FILE_OFFSET_BITS

/* What type should be used for uoff_t */
#undef UOFF_T_INT
#undef UOFF_T_LONG
#undef UOFF_T_LONG_LONG

/* printf()-format for uoff_t, eg. "u" or "lu" or "llu" */
#undef PRIuUOFF_T
