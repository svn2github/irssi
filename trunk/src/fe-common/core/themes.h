#ifndef __THEMES_H
#define __THEMES_H

typedef struct {
	char *name;

	int count;
	char **formats; /* in same order as in module's default formats */
	char **expanded_formats; /* this contains the formats after
				    expanding {templates} */
} MODULE_THEME_REC;

typedef struct {
	char *path;
	char *name;

	int default_color;
	GHashTable *modules;

	GSList *replace_keys;
	GSList *replace_values;
	GHashTable *abstracts;

	void *gui_data;
} THEME_REC;

typedef struct _FORMAT_REC FORMAT_REC;

extern GSList *themes;
extern THEME_REC *current_theme;
extern GHashTable *default_formats;

THEME_REC *theme_create(const char *path, const char *name);
void theme_destroy(THEME_REC *rec);

THEME_REC *theme_load(const char *name);

#define theme_register(formats) theme_register_module(MODULE_NAME, formats)
#define theme_unregister() theme_unregister_module(MODULE_NAME)
void theme_register_module(const char *module, FORMAT_REC *formats);
void theme_unregister_module(const char *module);

void themes_init(void);
void themes_deinit(void);

#endif
