#ifndef __TERMINFO_CORE_H
#define __TERMINFO_CORE_H

#include <termios.h>

#define terminfo_move(x, y) current_term->move(current_term, x, y)
#define terminfo_scroll(y1, y2, count) current_term->scroll(current_term, y1, y2, count)
#define terminfo_clear() current_term->clear(current_term)
#define terminfo_clrtoeol() current_term->clrtoeol(current_term)
#define terminfo_set_fg(color) current_term->set_fg(current_term, color)
#define terminfo_set_bg(color) current_term->set_bg(current_term, color)
#define terminfo_set_normal() current_term->set_normal(current_term)
#define terminfo_set_bold() current_term->set_bold(current_term)
#define terminfo_set_uline(set) current_term->set_uline(current_term, set)
#define terminfo_set_standout(set) current_term->set_standout(current_term, set)
#define terminfo_has_colors(term) (term->TI_fg[0] != NULL)

typedef struct _TERM_REC TERM_REC;

struct _TERM_REC {
        /* Functions */
	void (*move)(TERM_REC *term, int x, int y);
	void (*scroll)(TERM_REC *term, int y1, int y2, int count);

        void (*clear)(TERM_REC *term);
	void (*clrtoeol)(TERM_REC *term);

	void (*set_fg)(TERM_REC *term, int color);
	void (*set_bg)(TERM_REC *term, int color);
	void (*set_normal)(TERM_REC *term);
	void (*set_bold)(TERM_REC *term);
	void (*set_uline)(TERM_REC *term, int set);
	void (*set_standout)(TERM_REC *term, int set);

        void (*beep)(TERM_REC *term);

#ifndef HAVE_TERMINFO
	char buffer1[1024], buffer2[1024];
#endif
	FILE *in, *out;
	struct termios tio, old_tio;

        /* Terminal size */
        int width, height;

        /* Cursor movement */
	const char *TI_smcup, *TI_rmcup, *TI_cup;
	const char *TI_hpa, *TI_vpa;
        int TI_xhpa, TI_xvpa;

	/* Scrolling */
	const char *TI_csr, *TI_wind;
	const char *TI_ri, *TI_rin, *TI_ind, *TI_indn;
	const char *TI_il, *TI_il1, *TI_dl, *TI_dl1;

	/* Clearing screen */
	const char *TI_clear, *TI_ed; /* + *TI_dl, *TI_dl1; */

        /* Clearing to end of line */
	const char *TI_el;

	/* Colors */
	const char *TI_sgr0; /* turn off all attributes */
	const char *TI_smul, *TI_rmul; /* underline on/off */
        const char *TI_smso, *TI_rmso; /* standout on/off */
        const char *TI_bold, *TI_blink;
	const char *TI_setaf, *TI_setab, *TI_setf, *TI_setb;

        /* Colors - generated and dynamically allocated */
	char *TI_fg[16], *TI_bg[16], *TI_normal;

	/* Beep */
        char *TI_bel;
};

extern TERM_REC *current_term;

TERM_REC *terminfo_core_init(FILE *in, FILE *out);
void terminfo_core_deinit(TERM_REC *term);

/* Setup colors - if force is set, use ANSI-style colors if
   terminal capabilities don't contain color codes */
void terminfo_setup_colors(TERM_REC *term, int force);

/* Terminal was resized - ask the width/height from terminfo again */
void terminfo_resize(TERM_REC *term);

void terminfo_cont(TERM_REC *term);
void terminfo_stop(TERM_REC *term);

#endif
