#ifndef __IRSSI_CORE_H
#define __IRSSI_CORE_H

/* for determining what GUI is currently in use: */
#define IRSSI_GUI_NONE	0
#define IRSSI_GUI_TEXT	1
#define IRSSI_GUI_GTK	2
#define IRSSI_GUI_GNOME	3
#define IRSSI_GUI_QT   	4
#define IRSSI_GUI_KDE  	5

extern int irssi_gui;

void core_init_paths(int argc, char *argv[]);

void core_init(void);
void core_deinit(void);

#endif
