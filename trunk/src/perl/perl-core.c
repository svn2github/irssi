/*
 perl-core.c : irssi

    Copyright (C) 1999-2001 Timo Sirainen

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

#define NEED_PERL_H
#include "module.h"
#include "modules.h"
#include "core.h"
#include "signals.h"
#include "misc.h"
#include "settings.h"
#include "lib-config/iconfig.h" /* FIXME: remove before .99 */

#include "perl-core.h"
#include "perl-common.h"
#include "perl-signals.h"
#include "perl-sources.h"

#include "XSUB.h"
#include "irssi-core.pl.h"

/* For compatibility with perl 5.004 and older */
#ifndef HAVE_PL_PERL
#  define PL_perl_destruct_level perl_destruct_level
#endif

GSList *perl_scripts;
PerlInterpreter *my_perl;

static int print_script_errors;

#define IS_PERL_SCRIPT(file) \
	(strlen(file) > 3 && strcmp(file+strlen(file)-3, ".pl") == 0)

static void perl_script_destroy_package(PERL_SCRIPT_REC *script)
{
	dSP;

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(new_pv(script->package)));
	PUTBACK;

	perl_call_pv("Irssi::Core::destroy", G_VOID|G_EVAL|G_DISCARD);

	SPAGAIN;

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static void perl_script_destroy(PERL_SCRIPT_REC *script)
{
	perl_scripts = g_slist_remove(perl_scripts, script);

	signal_emit("script destroyed", 1, script);

	perl_signal_remove_script(script);
	perl_source_remove_script(script);

	g_free(script->name);
	g_free(script->package);
        g_free_not_null(script->path);
        g_free_not_null(script->data);
        g_free(script);
}

extern void boot_DynaLoader(CV* cv);

#if PERL_STATIC_LIBS == 1
extern void boot_Irssi(CV *cv);

XS(boot_Irssi_Core)
{
	dXSARGS;

	irssi_callXS(boot_Irssi, cv, mark);
        irssi_boot(Irc);
        irssi_boot(UI);
        irssi_boot(TextUI);
	XSRETURN_YES;
}
#endif

static void xs_init(void)
{
	dXSUB_SYS;

#if PERL_STATIC_LIBS == 1
	newXS("Irssi::Core::boot_Irssi_Core", boot_Irssi_Core, __FILE__);
#endif

	/* boot the dynaloader too, if we want to use some
	   other dynamic modules.. */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);
}

/* Initialize perl interpreter */
void perl_scripts_init(void)
{
	char *args[] = {"", "-e", "0"};
	char *code, *use_code;

	perl_scripts = NULL;
        perl_sources_start();
	perl_signals_start();

	my_perl = perl_alloc();
	perl_construct(my_perl);

	perl_parse(my_perl, xs_init, 3, args, NULL);
#if PERL_STATIC_LIBS == 1
	perl_eval_pv("Irssi::Core::boot_Irssi_Core();", TRUE);
#endif

        perl_common_start();

	use_code = perl_get_use_list();
        code = g_strdup_printf(irssi_core_code, PERL_STATIC_LIBS, use_code);
	perl_eval_pv(code, TRUE);

	g_free(code);
        g_free(use_code);
}

/* Destroy all perl scripts and deinitialize perl interpreter */
void perl_scripts_deinit(void)
{
	if (my_perl == NULL)
		return;

	/* unload all scripts */
        while (perl_scripts != NULL)
		perl_script_unload(perl_scripts->data);

        signal_emit("perl scripts deinit", 0);

        perl_signals_stop();
	perl_sources_stop();
	perl_common_stop();

	/* Unload all perl libraries loaded with dynaloader */
	perl_eval_pv("foreach my $lib (@DynaLoader::dl_modules) { if ($lib =~ /^Irssi\\b/) { $lib .= '::deinit();'; eval $lib; } }", TRUE);
	perl_eval_pv("eval { foreach my $lib (@DynaLoader::dl_librefs) { DynaLoader::dl_unload_file($lib); } }", TRUE);

	/* perl interpreter */
	perl_destruct(my_perl);
	perl_free(my_perl);
	my_perl = NULL;
}

/* Modify the script name so that all non-alphanumeric characters are
   translated to '_' */
void script_fix_name(char *name)
{
	char *p;

	p = strrchr(name, '.');
	if (p != NULL) *p = '\0';

	while (*name != '\0') {
		if (*name != '_' && !isalnum(*name))
			*name = '_';
		name++;
	}
}

static char *script_file_get_name(const char *path)
{
	char *name;

        name = g_strdup(g_basename(path));
	script_fix_name(name);
        return name;
}

static char *script_data_get_name(void)
{
	GString *name;
        char *ret;
	int n;

	name = g_string_new(NULL);
        n = 1;
	do {
		g_string_sprintf(name, "data%d", n);
                n++;
	} while (perl_script_find(name->str) != NULL);

	ret = name->str;
        g_string_free(name, FALSE);
        return ret;
}

static int perl_script_eval(PERL_SCRIPT_REC *script)
{
	dSP;
	char *error;
	int retcount;

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(new_pv(script->path != NULL ? script->path :
				 script->data)));
	XPUSHs(sv_2mortal(new_pv(script->name)));
	PUTBACK;

	retcount = perl_call_pv(script->path != NULL ?
				"Irssi::Core::eval_file" :
				"Irssi::Core::eval_data",
				G_EVAL|G_SCALAR);
	SPAGAIN;

        error = NULL;
	if (SvTRUE(ERRSV)) {
                error = SvPV(ERRSV, PL_na);
	} else if (retcount > 0) {
		error = POPp;
	}

	if (error != NULL) {
		if (*error == '\0')
			error = NULL;
		else
			signal_emit("script error", 2, script, error);
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

        return error == NULL;
}

/* NOTE: name must not be free'd */
static PERL_SCRIPT_REC *script_load(char *name, const char *path,
				    const char *data)
{
        PERL_SCRIPT_REC *script;

	/* if there's a script with a same name, destroy it */
	script = perl_script_find(name);
	if (script != NULL)
		perl_script_destroy(script);

	script = g_new0(PERL_SCRIPT_REC, 1);
	script->name = name;
	script->package = g_strdup_printf("Irssi::Script::%s", name);
	script->path = g_strdup(path);
        script->data = g_strdup(data);

	perl_scripts = g_slist_append(perl_scripts, script);
	signal_emit("script created", 1, script);

	if (!perl_script_eval(script))
                script = NULL; /* the script is destroyed in "script error" signal */
        return script;
}

/* Load a perl script, path must be a full path. */
PERL_SCRIPT_REC *perl_script_load_file(const char *path)
{
	char *name;

        g_return_val_if_fail(path != NULL, NULL);

        name = script_file_get_name(path);
        return script_load(name, path, NULL);
}

/* Load a perl script from given data */
PERL_SCRIPT_REC *perl_script_load_data(const char *data)
{
	char *name;

        g_return_val_if_fail(data != NULL, NULL);

	name = script_data_get_name();
	return script_load(name, NULL, data);
}

/* Unload perl script */
void perl_script_unload(PERL_SCRIPT_REC *script)
{
        g_return_if_fail(script != NULL);

	perl_script_destroy_package(script);
        perl_script_destroy(script);
}

/* Find loaded script by name */
PERL_SCRIPT_REC *perl_script_find(const char *name)
{
	GSList *tmp;

        g_return_val_if_fail(name != NULL, NULL);

	for (tmp = perl_scripts; tmp != NULL; tmp = tmp->next) {
		PERL_SCRIPT_REC *rec = tmp->data;

		if (strcmp(rec->name, name) == 0)
                        return rec;
	}

        return NULL;
}

/* Find loaded script by package */
PERL_SCRIPT_REC *perl_script_find_package(const char *package)
{
	GSList *tmp;

        g_return_val_if_fail(package != NULL, NULL);

	for (tmp = perl_scripts; tmp != NULL; tmp = tmp->next) {
		PERL_SCRIPT_REC *rec = tmp->data;

		if (strcmp(rec->package, package) == 0)
                        return rec;
	}

        return NULL;
}

/* Returns full path for the script */
char *perl_script_get_path(const char *name)
{
	struct stat statbuf;
	char *file, *path;

	if (g_path_is_absolute(name) || (name[0] == '~' && name[1] == '/')) {
		/* full path specified */
                return convert_home(name);
	}

	/* add .pl suffix if it's missing */
	file = IS_PERL_SCRIPT(name) ? g_strdup(name) :
		g_strdup_printf("%s.pl", name);

	/* check from ~/.irssi/scripts/ */
	path = g_strdup_printf("%s/scripts/%s", get_irssi_dir(), file);
	if (stat(path, &statbuf) != 0) {
		/* check from SCRIPTDIR */
		g_free(path);
		path = g_strdup_printf(SCRIPTDIR"/%s", file);
		if (stat(path, &statbuf) != 0)
                        path = NULL;
	}
	g_free(file);
        return path;
}

/* If core should handle printing script errors */
void perl_core_print_script_error(int print)
{
        print_script_errors = print;
}

/* Returns the perl module's API version. */
int perl_get_api_version(void)
{
        return IRSSI_PERL_API_VERSION;
}

static void perl_scripts_autorun(void)
{
	DIR *dirp;
	struct dirent *dp;
	struct stat statbuf;
	char *path, *fname;

        /* run *.pl scripts from ~/.irssi/scripts/autorun/ */
	path = g_strdup_printf("%s/scripts/autorun", get_irssi_dir());
	dirp = opendir(path);
	if (dirp == NULL) {
		g_free(path);
		return;
	}

	while ((dp = readdir(dirp)) != NULL) {
		if (!IS_PERL_SCRIPT(dp->d_name))
			continue;

		fname = g_strdup_printf("%s/%s", path, dp->d_name);
		if (stat(fname, &statbuf) == 0 && !S_ISDIR(statbuf.st_mode))
			perl_script_load_file(fname);
		g_free(fname);
	}
	closedir(dirp);
	g_free(path);
}

static void sig_script_error(PERL_SCRIPT_REC *script, const char *error)
{
	char *str;

	if (print_script_errors) {
		str = g_strdup_printf("Script '%s' error:",
				      script == NULL ? "??" : script->name);
		signal_emit("gui dialog", 2, "error", str);
		signal_emit("gui dialog", 2, "error", error);
                g_free(str);
	}

	if (script != NULL) {
		perl_script_unload(script);
                signal_stop();
	}
}

static void sig_autorun(void)
{
	signal_remove("irssi init finished", (SIGNAL_FUNC) sig_autorun);

        perl_scripts_autorun();
}

void perl_core_init(void)
{
	/* FIXME: remove before .99 - backwards compatibility
	   fix between CVS versions */
	CONFIG_NODE *node;

	node = iconfig_node_traverse("settings", FALSE);
	if (node != NULL)
                node = config_node_section(node, "perl/core", -1);
	if (node != NULL) {
                g_free(node->key);
		node->key = g_strdup("perl/core/scripts");
	}
        /* ----- */

        print_script_errors = 1;
	settings_add_str("perl", "perl_use_lib", PERL_USE_LIB);

	PL_perl_destruct_level = 1;
	perl_signals_init();
        signal_add_last("script error", (SIGNAL_FUNC) sig_script_error);

	perl_scripts_init();

	if (irssi_init_finished)
		perl_scripts_autorun();
	else {
		signal_add("irssi init finished", (SIGNAL_FUNC) sig_autorun);
		settings_check();
	}

	module_register("perl", "core");
}

void perl_core_deinit(void)
{
	perl_signals_deinit();
        perl_scripts_deinit();

	signal_remove("script error", (SIGNAL_FUNC) sig_script_error);
}
