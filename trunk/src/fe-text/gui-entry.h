#ifndef __GUI_ENTRY_H
#define __GUI_ENTRY_H

typedef struct {
	GString *text;
	int xpos, ypos, width; /* entry position in screen */
	int pos, scrstart, scrpos; /* cursor position */
        int hidden; /* print the chars as spaces in input line (useful for passwords) */

	int promptlen;
	char *prompt;

	int redraw_needed_from;
} GUI_ENTRY_REC;

extern GUI_ENTRY_REC *active_entry;

GUI_ENTRY_REC *gui_entry_create(int xpos, int ypos, int width);
void gui_entry_destroy(GUI_ENTRY_REC *entry);
void gui_entry_move(GUI_ENTRY_REC *entry, int xpos, int ypos, int width);

void gui_entry_set_active(GUI_ENTRY_REC *entry);

void gui_entry_set_prompt(GUI_ENTRY_REC *entry, const char *str);
void gui_entry_set_hidden(GUI_ENTRY_REC *entry, int hidden);

void gui_entry_set_text(GUI_ENTRY_REC *entry, const char *str);
char *gui_entry_get_text(GUI_ENTRY_REC *entry);

void gui_entry_insert_text(GUI_ENTRY_REC *entry, const char *str);
void gui_entry_insert_char(GUI_ENTRY_REC *entry, char chr);

void gui_entry_erase(GUI_ENTRY_REC *entry, int size);
void gui_entry_erase_word(GUI_ENTRY_REC *entry, int to_space);
void gui_entry_erase_next_word(GUI_ENTRY_REC *entry, int to_space);

int gui_entry_get_pos(GUI_ENTRY_REC *entry);
void gui_entry_set_pos(GUI_ENTRY_REC *entry, int pos);
void gui_entry_move_pos(GUI_ENTRY_REC *entry, int pos);
void gui_entry_move_words(GUI_ENTRY_REC *entry, int count, int to_space);

void gui_entry_redraw(GUI_ENTRY_REC *entry);

#endif
