#ifndef __ICONFIG_H
#define __ICONFIG_H

enum {
        NODE_TYPE_KEY,
        NODE_TYPE_VALUE,
        NODE_TYPE_BLOCK,
        NODE_TYPE_LIST,
	NODE_TYPE_COMMENT,
};

#define has_node_value(a) \
	((a)->type == NODE_TYPE_KEY || (a)->type == NODE_TYPE_VALUE)
#define is_node_list(a) \
        ((a)->type == NODE_TYPE_BLOCK || (a)->type == NODE_TYPE_LIST)

typedef struct {
	int type;
        char *key;
	void *value;
} CONFIG_NODE;

/* a = { x=y; y=z; }

   node1: type = NODE_TYPE_BLOCK, key = "a", value = (GSList *) nodes
   nodes: (node2, node3)
     node2: type = NODE_TYPE_KEY, key = "x", value = (char *) "y"
     node3: type = NODE_TYPE_KEY, key = "y", value = (char *) "z"

   b = ( a, { b=c; d=e; } )

   node1: type = NODE_TYPE_LIST, key = "b", value = (GSList *) nodes
   nodes: (node2, node3)
     node2: type = NODE_TYPE_VALUE, key = NULL, value = (char *) "a"
     node4: type = NODE_TYPE_BLOCK, key = NULL, value = (GSList *) nodes2
     nodes2: (node4, node5)
       node4: type = NODE_TYPE_KEY, key = "b", value = (char *) "c"
       node5: type = NODE_TYPE_KEY, key = "d", value = (char *) "e"

   Comments node has key=NULL and value is the comment line. Empty lines are
   also in comments so they won't be forgotten when the config file is
   written.

*/

struct _config_rec {
	char *fname;
	int handle;
        int create_mode;

	char *last_error;
	CONFIG_NODE *mainnode;
	GHashTable *cache;

	GScanner *scanner;

	/* while writing to configuration file.. */
	int tmp_indent_level; /* indentation position */
	int tmp_last_lf; /* last character was a line feed */
};

typedef struct _config_rec CONFIG_REC;

/* Open configuration. The file is created if it doesn't exist, unless
   `create_mode' is -1. `fname' can be NULL if you just want to use
   config_parse_data() */
CONFIG_REC *config_open(const char *fname, int create_mode);
/* Release all memory used by configuration */
void config_close(CONFIG_REC *rec);
/* Change file name of config file */
void config_change_file_name(CONFIG_REC *rec, const char *fname, int create_mode);

/* Parse configuration file */
int config_parse(CONFIG_REC *rec);
/* Parse configuration found from `data'. `input_name' is specifies the
   "configuration name" which is displayed in error messages. */
int config_parse_data(CONFIG_REC *rec, const char *data, const char *input_name);

/* Write configuration file. Write to `fname' if it's not NULL.
   If `create_mode' is -1, use the one that was given to config_open(). */
int config_write(CONFIG_REC *rec, const char *fname, int create_mode);

#define config_last_error(rec) \
    (rec)->last_error

/* Getting values

   `section' is something like "maingroup/key/subkey", or with lists
   "maingroup/(list/subkey"

   `def' is returned if the value is not found. */
char *config_get_str(CONFIG_REC *rec, const char *section, const char *key, const char *def);
int config_get_int(CONFIG_REC *rec, const char *section, const char *key, int def);
int config_get_bool(CONFIG_REC *rec, const char *section, const char *key, int def);

/* Return value of key `value_key' from list item where `key' is `value' */
const char *config_list_find(CONFIG_REC *rec, const char *section, const char *key, const char *value, const char *value_key);
/* Like config_list_find(), but return node instead of it's value */
CONFIG_NODE *config_list_find_node(CONFIG_REC *rec, const char *section, const char *key, const char *value, const char *value_key);

/* Setting values */
int config_set_str(CONFIG_REC *rec, const char *section, const char *key, const char *value);
int config_set_int(CONFIG_REC *rec, const char *section, const char *key, int value);
int config_set_bool(CONFIG_REC *rec, const char *section, const char *key, int value);

/* Handling the configuration directly with nodes -
   useful when you need to read all values in a block/list. */
CONFIG_NODE *config_node_find(CONFIG_NODE *node, const char *key);
/* Find the section from node - if not found create it unless new_type is -1.
   You can also specify in new_type if it's NODE_TYPE_LIST or NODE_TYPE_BLOCK */
CONFIG_NODE *config_node_section(CONFIG_REC *rec, CONFIG_NODE *parent, const char *key, int new_type);
/* Find the section with the whole path.
   Create the path if necessary `create' is TRUE. */
CONFIG_NODE *config_node_traverse(CONFIG_REC *rec, const char *section, int create);
/* Get the value of keys `key' and `key_value' and put them to
   `ret_key' and `ret_value'. Returns -1 if not found. */
int config_node_get_keyvalue(CONFIG_NODE *node, const char *key, const char *value_key, char **ret_key, char **ret_value);

char *config_node_get_str(CONFIG_NODE *parent, const char *key, const char *def);
int config_node_get_int(CONFIG_NODE *parent, const char *key, int def);
int config_node_get_bool(CONFIG_NODE *parent, const char *key, int def);

void config_node_set_str(CONFIG_NODE *parent, const char *key, const char *value);
void config_node_set_int(CONFIG_NODE *parent, const char *key, int value);
void config_node_set_bool(CONFIG_NODE *parent, const char *key, int value);

/* add/change the value of the `key' */
void config_node_set_str(CONFIG_NODE *parent, const char *key, const char *value);
/* remove one node from block/list.
   ..set_str() with value = NULL does the same. */
void config_node_remove(CONFIG_NODE *parent, CONFIG_NODE *node);

/* clear the entire configuration */
void config_nodes_remove_all(CONFIG_REC *rec);

#endif
