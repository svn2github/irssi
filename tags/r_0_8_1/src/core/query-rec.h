/* QUERY_REC definition, used for inheritance */

#include "window-item-rec.h"

char *address;
char *server_tag;
unsigned int unwanted:1; /* TRUE if the other side closed or
			    some error occured (DCC chats!) */
unsigned int destroying:1;
