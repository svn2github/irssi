#include "module.h"

MODULE = Irssi::UI::Themes  PACKAGE = Irssi
PROTOTYPES: ENABLE

void
theme_register(formats)
	SV *formats
PREINIT:
        STRLEN n_a;
	AV *av;
	FORMAT_REC *formatrecs;
	char *key, *value;
	int len, n, fpos;
CODE:

        if (!SvROK(formats))
        	croak("formats is not a reference to list");
	av = (AV *) SvRV(formats);
	len = av_len(av)+1;
	if (len == 0 || (len & 1) != 0)
        	croak("formats list is invalid - not dividable by 3 (%d)", len);

	formatrecs = g_new0(FORMAT_REC, len/2+2);
	formatrecs[0].tag = g_strdup(perl_get_package());
	formatrecs[0].def = g_strdup("Perl script");

        for (fpos = 1, n = 0; n < len; n++, fpos++) {
		key = SvPV(*av_fetch(av, n, 0), n_a); n++;
		value = SvPV(*av_fetch(av, n, 0), n_a);

		formatrecs[fpos].tag = g_strdup(key);
		formatrecs[fpos].def = g_strdup(value);
		formatrecs[fpos].params = MAX_FORMAT_PARAMS;
	}

	theme_register_module(perl_get_package(), formatrecs);

void
printformat(level, format, ...)
	int level
	char *format
PREINIT:
        STRLEN n_a;
	TEXT_DEST_REC dest;
	char *arglist[MAX_FORMAT_PARAMS];
	int n;
CODE:
	format_create_dest(&dest, NULL, NULL, level, NULL);
	memset(arglist, 0, sizeof(arglist));
	for (n = 2; n < 2+MAX_FORMAT_PARAMS; n++) {
		arglist[n-2] = n < items ? SvPV(ST(n), n_a) : "";
	}

        printformat_perl(&dest, format, arglist);

#*******************************
MODULE = Irssi::UI::Themes  PACKAGE = Irssi::Server
#*******************************

void
printformat(server, target, level, format, ...)
	Irssi::Server server
	char *target
	int level
	char *format
PREINIT:
        STRLEN n_a;
	TEXT_DEST_REC dest;
	char *arglist[MAX_FORMAT_PARAMS];
	int n;
CODE:
	format_create_dest(&dest, server, target, level, NULL);
	memset(arglist, 0, sizeof(arglist));
	for (n = 4; n < 4+MAX_FORMAT_PARAMS; n++) {
		arglist[n-4] = n < items ? SvPV(ST(n), n_a) : "";
	}

        printformat_perl(&dest, format, arglist);

#*******************************
MODULE = Irssi::UI::Themes  PACKAGE = Irssi::UI::Window
#*******************************

void
printformat(window, level, format, ...)
	Irssi::UI::Window window
	int level
	char *format
PREINIT:
        STRLEN n_a;
	TEXT_DEST_REC dest;
	char *arglist[MAX_FORMAT_PARAMS];
	int n;
CODE:
	format_create_dest(&dest, NULL, NULL, level, window);
	memset(arglist, 0, sizeof(arglist));
	for (n = 3; n < 3+MAX_FORMAT_PARAMS; n++) {
		arglist[n-3] = n < items ? SvPV(ST(n), n_a) : "";
	}

        printformat_perl(&dest, format, arglist);

#*******************************
MODULE = Irssi::UI::Themes  PACKAGE = Irssi::Windowitem
#*******************************

void
printformat(item, level, format, ...)
	Irssi::Windowitem item
	int level
	char *format
PREINIT:
        STRLEN n_a;
	TEXT_DEST_REC dest;
	char *arglist[MAX_FORMAT_PARAMS];
	int n;
CODE:
	format_create_dest(&dest, item->server, item->name, level, NULL);
	memset(arglist, 0, sizeof(arglist));
	for (n = 3; n < 3+MAX_FORMAT_PARAMS; n++) {
		arglist[n-3] = n < items ? SvPV(ST(n), n_a) : "";
	}

        printformat_perl(&dest, format, arglist);

