#ifndef __COMPLETION_H
#define __COMPLETION_H

#include "window-items.h"

int completion_msgtoyou(SERVER_REC *server, const char *msg);
char *completion_line(WINDOW_REC *window, const char *line, int *pos);
char *auto_completion(const char *line, int *pos);

void completion_init(void);
void completion_deinit(void);

#endif
