/*
 irssi.c : irssi

    Copyright (C) 1999-2000 Timo Sirainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "module-formats.h"
#include "modules-load.h"
#include "args.h"
#include "signals.h"
#include "levels.h"
#include "core.h"
#include "settings.h"
#include "session.h"

#include "printtext.h"
#include "fe-common-core.h"
#include "themes.h"

#include "term.h"
#include "gui-entry.h"
#include "mainwindows.h"
#include "gui-printtext.h"
#include "gui-readline.h"
#include "statusbar.h"
#include "gui-windows.h"
#include "textbuffer-reformat.h"

#include <signal.h>

#ifdef HAVE_STATIC_PERL
void perl_core_init(void);
void perl_core_deinit(void);

void fe_perl_init(void);
void fe_perl_deinit(void);
#endif

void irc_init(void);
void irc_deinit(void);

void fe_common_irc_init(void);
void fe_common_irc_deinit(void);

void gui_expandos_init(void);
void gui_expandos_deinit(void);

void textbuffer_commands_init(void);
void textbuffer_commands_deinit(void);

void lastlog_init(void);
void lastlog_deinit(void);

void mainwindow_activity_init(void);
void mainwindow_activity_deinit(void);

void mainwindows_layout_init(void);
void mainwindows_layout_deinit(void);

void term_dummy_init(void);
void term_dummy_deinit(void);

static int dirty, full_redraw, dummy;

static GMainLoop *main_loop;
int quitting;

static const char *firsttimer_text =
	"Looks like this is the first time you run irssi.\n"
	"This is just a reminder that you really should go read\n"
	"startup-HOWTO if you haven't already. Irssi's default\n"
	"settings aren't probably what you've used to, and you\n"
	"shouldn't judge the whole client as crap based on them.\n\n"
	"You can find startup-HOWTO and more irssi beginner info at\n"
	"http://irssi.org/beginner/";
static int display_firsttimer = FALSE;


static void sig_exit(void)
{
        quitting = TRUE;
}

/* redraw irssi's screen.. */
void irssi_redraw(void)
{
	dirty = TRUE;
        full_redraw = TRUE;
}

void irssi_set_dirty(void)
{
        dirty = TRUE;
}

static void dirty_check(void)
{
	if (!dirty || dummy)
		return;

        term_resize_dirty();

	if (full_redraw) {
                full_redraw = FALSE;

		/* first clear the screen so curses will be
		   forced to redraw the screen */
		term_clear();
		term_refresh(NULL);

		mainwindows_redraw();
		statusbar_redraw(NULL, TRUE);
	}

	mainwindows_redraw_dirty();
        statusbar_redraw_dirty();
	term_refresh(NULL);

        dirty = FALSE;
}

static void textui_init(void)
{
	irssi_gui = IRSSI_GUI_TEXT;
	core_init();
	irc_init();
	fe_common_core_init();
	fe_common_irc_init();

	theme_register(gui_text_formats);
	signal_add_last("gui exit", (SIGNAL_FUNC) sig_exit);
}

static void textui_finish_init(void)
{
	quitting = FALSE;

	if (dummy)
		term_dummy_init();
	else {
		term_refresh_freeze();
	        textbuffer_init();
	        textbuffer_view_init();
		textbuffer_commands_init();
		textbuffer_reformat_init();
		gui_expandos_init();
		gui_printtext_init();
		gui_readline_init();
	        lastlog_init();
		mainwindows_init();
		mainwindow_activity_init();
		mainwindows_layout_init();
		gui_windows_init();
		statusbar_init();
		term_refresh_thaw();
	}

	settings_check();
	module_register("core", "fe-text");

#ifdef HAVE_STATIC_PERL
	perl_core_init();
	fe_perl_init();
#endif

	dirty_check();

	fe_common_core_finish_init();
	signal_emit("irssi init finished", 0);

	if (display_firsttimer) {
		printtext_window(active_win, MSGLEVEL_CLIENTNOTICE,
				 "%s", firsttimer_text);
	}
}

static void textui_deinit(void)
{
	signal(SIGINT, SIG_DFL);

        term_refresh_freeze();
	while (modules != NULL)
		module_unload(modules->data);

#ifdef HAVE_STATIC_PERL
        perl_core_deinit();
        fe_perl_deinit();
#endif

        dirty_check(); /* one last time to print any quit messages */
	signal_remove("gui exit", (SIGNAL_FUNC) sig_exit);

	if (dummy)
		term_dummy_deinit();
	else {
	        lastlog_deinit();
		statusbar_deinit();
		gui_printtext_deinit();
		gui_readline_deinit();
		gui_windows_deinit();
		mainwindows_layout_deinit();
		mainwindow_activity_deinit();
		mainwindows_deinit();
		gui_expandos_deinit();
		textbuffer_reformat_deinit();
		textbuffer_commands_deinit();
	        textbuffer_view_deinit();
	        textbuffer_deinit();

	        term_refresh_thaw();
	        term_deinit();
	}

	theme_unregister();

	fe_common_irc_deinit();
	fe_common_core_deinit();
	irc_deinit();
	core_deinit();
}

static void check_oldcrap(void)
{
        FILE *f;
	char *path, str[256];
        int found;

        /* check that default.theme is up-to-date */
	path = g_strdup_printf("%s/default.theme", get_irssi_dir());
	f = fopen(path, "r+");
	if (f == NULL) {
		g_free(path);
                return;
	}
        found = FALSE;
	while (!found && fgets(str, sizeof(str), f) != NULL)
                found = strstr(str, "abstracts = ") != NULL;
	fclose(f);

	if (found) {
		g_free(path);
		return;
	}

	printf("\nYou seem to have old default.theme in "IRSSI_DIR_SHORT"/ directory.\n");
        printf("Themeing system has changed a bit since last irssi release,\n");
        printf("you should either delete your old default.theme or manually\n");
        printf("merge it with the new default.theme.\n\n");
	printf("Do you want to delete the old theme now? (Y/n)\n");

	str[0] = '\0';
	fgets(str, sizeof(str), stdin);
	if (i_toupper(str[0]) == 'Y' || str[0] == '\n' || str[0] == '\0')
                remove(path);
	g_free(path);
}

static void check_files(void)
{
	struct stat statbuf;

	if (stat(get_irssi_dir(), &statbuf) != 0) {
		/* ~/.irssi doesn't exist, first time running irssi */
		display_firsttimer = TRUE;
	} else {
                check_oldcrap();
	}
}

#ifdef WIN32
static void winsock_init(void)
{
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("Error initializing winsock\n");
		exit(1);
	}
}
#endif

int main(int argc, char **argv)
{
	static struct poptOption options[] = {
		{ "dummy", 'd', POPT_ARG_NONE, &dummy, 0, "Use the dummy terminal mode", NULL },
		{ NULL, '\0', 0, NULL }
	};

	dummy = FALSE;
	quitting = FALSE;
	core_init_paths(argc, argv);

	check_files();

#ifdef WIN32
        winsock_init();
#endif
#ifdef HAVE_SOCKS
	SOCKSinit(argv[0]);
#endif
#ifdef ENABLE_NLS
	/* initialize the i18n stuff */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
#endif

	textui_init();
	args_register(options);
	args_execute(argc, argv);

	if (!dummy && !term_init()) {
		fprintf(stderr, "Can't initialize screen handling, quitting.\n");
		fprintf(stderr, "You can still use the dummy mode with -d parameter\n");
		return 1;
	}

	textui_finish_init();
	main_loop = g_main_new(TRUE);

	/* Does the same as g_main_run(main_loop), except we
	   can call our dirty-checker after each iteration */
	while (!quitting) {
		g_main_iteration(TRUE);
                dirty_check();
	}

	g_main_destroy(main_loop);
	textui_deinit();

        session_upgrade(); /* if we /UPGRADEd, start the new process */
	return 0;
}
