#ifndef __MISC_H
#define __MISC_H

/* `str' should be type char[MAX_INT_STRLEN] */
#define ltoa(str, num) \
	g_snprintf(str, sizeof(str), "%d", num)

typedef void* (*FOREACH_FIND_FUNC) (void *item, void *data);
typedef int (*COLUMN_LEN_FUNC)(void *data);

int g_timeval_cmp(const GTimeVal *tv1, const GTimeVal *tv2);
long get_timeval_diff(const GTimeVal *tv1, const GTimeVal *tv2);

/* find `item' from a space separated `list' */
int find_substr(const char *list, const char *item);
/* return how many items `array' has */
int strarray_length(char **array);
/* return index of `item' in `array' or -1 if not found */
int strarray_find(char **array, const char *item);

int execute(const char *cmd); /* returns pid or -1 = error */

GSList *gslist_find_string(GSList *list, const char *key);
GSList *gslist_find_icase_string(GSList *list, const char *key);
GList *glist_find_string(GList *list, const char *key);
GList *glist_find_icase_string(GList *list, const char *key);

void *gslist_foreach_find(GSList *list, FOREACH_FIND_FUNC func, const void *data);

/* `list' contains pointer to structure with a char* to string. */
char *gslistptr_to_string(GSList *list, int offset, const char *delimiter);
/* `list' contains char* */
char *gslist_to_string(GSList *list, const char *delimiter);

/* save all keys in hash table to linked list - you shouldn't remove any
   items while using this list, use g_slist_free() after you're done with it */
GSList *hashtable_get_keys(GHashTable *hash);

/* strstr() with case-ignoring */
char *stristr(const char *data, const char *key);

/* like strstr(), but matches only for full words.
   `icase' specifies if match is case sensitive */
char *strstr_full_case(const char *data, const char *key, int icase);
char *strstr_full(const char *data, const char *key);
char *stristr_full(const char *data, const char *key);

/* easy way to check if regexp matches */
int regexp_match(const char *str, const char *regexp);

/* Create the directory and all it's parent directories */
int mkpath(const char *path, int mode);
/* convert ~/ to $HOME */
char *convert_home(const char *path);

/* Case-insensitive string hash functions */
int g_istr_equal(gconstpointer v, gconstpointer v2);
unsigned int g_istr_hash(gconstpointer v);

/* Case-insensitive GCompareFunc func */
int g_istr_cmp(gconstpointer v, gconstpointer v2);

/* Find `mask' from `data', you can use * and ? wildcards. */
int match_wildcards(const char *mask, const char *data);

/* Return TRUE if all characters in `str' are numbers.
   Stop when `end_char' is found from string. */
int is_numeric(const char *str, char end_char);

/* replace all `from' chars in string to `to' chars. returns `str' */
char *replace_chars(char *str, char from, char to);

/* octal <-> decimal conversions */
int octal2dec(int octal);
int dec2octal(int decimal);

/* convert all low-ascii (<32) to ^<A..> combinations */
char *show_lowascii(const char *channel);

/* Get time in human readable form with localtime() + asctime() */
char *my_asctime(time_t t);

/* Returns number of columns needed to print items.
   save_column_widths is filled with length of each column. */
int get_max_column_count(GSList *items, COLUMN_LEN_FUNC len_func,
			 int max_width, int max_columns,
			 int item_extra, int item_min_size,
			 int **save_column_widths, int *rows);

/* Return a column sorted copy of a list. */
GSList *columns_sort_list(GSList *list, int rows);

#endif
