/*
   This file is part of phpurple

   Copyright (C) 2007 Anatoliy Belsky

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

   In addition, as a special exception, the copyright holders of phpurple
   give you permission to combine phpurple with code included in the
   standard release of PHP under the PHP license (or modified versions of
   such code, with unchanged license). You may copy and distribute such a
   system following the terms of the GNU GPL for phpurple and the licenses
   of the other code concerned, provided that you include the source code of
   that other code when and as the GNU GPL requires distribution of source code.

   You must obey the GNU General Public License in all respects for all of the
   code used other than standard release of PHP. If you modify this file, you
   may extend this exception to your version of the file, but you are not
   obligated to do so. If you do not wish to do so, delete this exception
   statement from your version.

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_purple.h"

#include <glib.h>
// 
// #include <string.h>
// #include <unistd.h>

#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "pounce.h"
#include "savedstatuses.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"
#include "version.h"

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#include <sys/wait.h>
#endif

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

static void php_ui_init();
static zval *purple_php_string_zval(const char *str);
static zval *purple_php_long_zval(long l);
static void purple_glib_io_destroy(gpointer data);
static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data);
static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							gpointer data);
static void
purple_php_write_conv_function(PurpleConversation *conv, const char *who, const char *alias,
							   const char *message, PurpleMessageFlags flags, time_t mtime);
static void purple_php_signed_on_function(PurpleConnection *gc, gpointer null);
#ifdef HAVE_SIGNAL_H
static void sighandler(int sig);
static void clean_pid();
#endif

static zval *purple_php_write_conv_function_name;
static PurpleSavedStatus* saved_status;

#ifdef HAVE_SIGNAL_H
static char *segfault_message;

static int catch_sig_list[] = {
	SIGSEGV,
	SIGHUP,
	SIGINT,
	SIGTERM,
	SIGQUIT,
	SIGCHLD,
	SIGALRM,
	-1
};

static int ignore_sig_list[] = {
	SIGPIPE,
	-1
};
#endif

typedef struct _PurpleGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} PurpleGLibIOClosure;

static PurpleEventLoopUiOps glib_eventloops = 
{
	g_timeout_add,
	g_source_remove,
	glib_input_add,
	g_source_remove,
	NULL,
#if GLIB_CHECK_VERSION(2,14,0)
	g_timeout_add_seconds,
#else
	NULL,
#endif
	
	/* padding */
	NULL,
	NULL,
	NULL
};

/*** Conversation uiops ***/
static PurpleConversationUiOps php_conv_uiops =
{
	NULL,                      /* create_conversation  */
	NULL,                      /* destroy_conversation */
	NULL,            /* write_chat           */
	NULL,              /* write_im             */
	purple_php_write_conv_function,            /* write_conv           */
	NULL,                      /* chat_add_users       */
	NULL,                      /* chat_rename_user     */
	NULL,                      /* chat_remove_users    */
	NULL,                      /* chat_update_user     */
	NULL,                      /* present              */
	NULL,                      /* has_focus            */
	NULL,                      /* custom_smiley_add    */
	NULL,                      /* custom_smiley_write  */
	NULL,                      /* custom_smiley_close  */
	NULL,                      /* send_confirm         */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurpleCoreUiOps php_core_uiops = 
{
	NULL,
	NULL,
	php_ui_init,
	NULL,
	
	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

ZEND_DECLARE_MODULE_GLOBALS(purple);

/* True global resources - no need for thread safety here */
static int le_purple;

/* {{{ purple_functions[]
 *
 * Every user visible function must have an entry in purple_functions[].
 */
zend_function_entry purple_functions[] = {
	PHP_FE(purple_core_get_version, NULL)
	PHP_FE(purple_core_init, NULL)
	
	PHP_FE(purple_plugins_get_protocols, NULL)
	PHP_FE(purple_plugins_add_search_path, NULL)
	PHP_FE(purple_plugins_load_saved, NULL)
	
	PHP_FE(purple_account_new, NULL)
	PHP_FE(purple_account_set_password, NULL)
	PHP_FE(purple_account_set_enabled, NULL)
	PHP_FE(purple_account_is_connected, NULL)
	
	PHP_FE(purple_util_set_user_dir, NULL)
	
	PHP_FE(purple_savedstatus_new, NULL)
	PHP_FE(purple_savedstatus_activate, NULL)

	PHP_FE(purple_conversation_get_name, NULL)
	PHP_FE(purple_conversation_write, NULL)
	PHP_FE(purple_conversation_new, NULL)
	PHP_FE(purple_conv_im_send, NULL)
	PHP_FE(purple_conversation_set_account, NULL)
			
	PHP_FE(purple_signal_connect, NULL)
			
	PHP_FE(purple_blist_load, NULL)
	PHP_FE(purple_find_buddy, NULL)
	PHP_FE(purple_blist_new, NULL)
			
	PHP_FE(purple_prefs_load, NULL)
			
	PHP_FE(purple_pounces_load, NULL)
			
	/*not purple functions*/
	PHP_FE(purple_loop, NULL)
	PHP_FE(purple_php_write_conv_function, NULL)
		
	{NULL, NULL, NULL}	/* Must be the last line in purple_functions[] */
};
/* }}} */

/* {{{ purple_module_entry
 */
zend_module_entry purple_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,	
#endif
	"purple",
	purple_functions,
	PHP_MINIT(purple),
	PHP_MSHUTDOWN(purple),
	PHP_RINIT(purple),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(purple),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(purple),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PURPLE
ZEND_GET_MODULE(purple)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
//     STD_PHP_INI_ENTRY("purple.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_purple_globals, purple_globals)
//     STD_PHP_INI_ENTRY("purple.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.custom_user_directory", "/dev/null", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.custom_plugin_path", "", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.ui_id", "php", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.debug_enabled", "1", PHP_INI_ALL, OnUpdateString, global_value, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.plugin_save_pref", "/purple/nullclient/plugins/saved", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
PHP_INI_END()

/* }}} */

/* {{{ php_purple_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_purple_init_globals(zend_purple_globals *purple_globals)
{
	purple_globals->global_value = 0;
	purple_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(purple)
{
	REGISTER_INI_ENTRIES();

	/* A primitive defining the basic structure of a status type */
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_UNSET",         PURPLE_STATUS_UNSET,         CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_OFFLINE",       PURPLE_STATUS_OFFLINE,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_AVAILABLE",     PURPLE_STATUS_AVAILABLE,     CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_UNAVAILABLE",   PURPLE_STATUS_UNAVAILABLE,   CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_INVISIBLE",     PURPLE_STATUS_INVISIBLE,     CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_AWAY",          PURPLE_STATUS_AWAY,          CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_EXTENDED_AWAY", PURPLE_STATUS_EXTENDED_AWAY, CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_MOBILE",        PURPLE_STATUS_MOBILE,        CONST_CS | CONST_PERSISTENT );
	/* A type of conversation */
	REGISTER_LONG_CONSTANT("PURPLE_CONV_TYPE_UNKNOWN",    PURPLE_CONV_TYPE_UNKNOWN,    CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_TYPE_IM",         PURPLE_CONV_TYPE_IM,         CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_TYPE_CHAT",       PURPLE_CONV_TYPE_CHAT,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_TYPE_MISC",       PURPLE_CONV_TYPE_MISC,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_TYPE_ANY",        PURPLE_CONV_TYPE_ANY,        CONST_CS | CONST_PERSISTENT );
	/* Conversation update type */
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_ADD",      PURPLE_CONV_UPDATE_ADD,      CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_REMOVE",   PURPLE_CONV_UPDATE_REMOVE,   CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_ACCOUNT",  PURPLE_CONV_UPDATE_ACCOUNT,  CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_TYPING",   PURPLE_CONV_UPDATE_TYPING,   CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_UNSEEN",   PURPLE_CONV_UPDATE_UNSEEN,   CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_LOGGING",  PURPLE_CONV_UPDATE_LOGGING,  CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_TOPIC",    PURPLE_CONV_UPDATE_TOPIC,    CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_ACCOUNT_ONLINE",  PURPLE_CONV_ACCOUNT_ONLINE,  CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_ACCOUNT_OFFLINE", PURPLE_CONV_ACCOUNT_OFFLINE, CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_AWAY",     PURPLE_CONV_UPDATE_AWAY,     CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_ICON",     PURPLE_CONV_UPDATE_ICON,     CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_TITLE",    PURPLE_CONV_UPDATE_TITLE,    CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_CHATLEFT", PURPLE_CONV_UPDATE_CHATLEFT, CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_CONV_UPDATE_FEATURES", PURPLE_CONV_UPDATE_FEATURES, CONST_CS | CONST_PERSISTENT );
	/* Flags applicable to a message */
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_SEND",         PURPLE_MESSAGE_SEND,         CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_RECV",         PURPLE_MESSAGE_RECV,         CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_SYSTEM",       PURPLE_MESSAGE_SYSTEM,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_AUTO_RESP",    PURPLE_MESSAGE_AUTO_RESP,    CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_ACTIVE_ONLY",  PURPLE_MESSAGE_ACTIVE_ONLY,  CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_NICK",         PURPLE_MESSAGE_NICK,         CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_NO_LOG",       PURPLE_MESSAGE_NO_LOG,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_WHISPER",      PURPLE_MESSAGE_WHISPER,      CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_ERROR",        PURPLE_MESSAGE_ERROR,        CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_DELAYED",      PURPLE_MESSAGE_DELAYED,      CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_RAW",          PURPLE_MESSAGE_RAW,          CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_IMAGES",       PURPLE_MESSAGE_IMAGES,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_NOTIFY",       PURPLE_MESSAGE_NOTIFY,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_NO_LINKIFY",   PURPLE_MESSAGE_NO_LINKIFY,   CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_MESSAGE_INVISIBLE",    PURPLE_MESSAGE_INVISIBLE,    CONST_CS | CONST_PERSISTENT );

#ifdef HAVE_SIGNAL_H
	int sig_indx;	/* for setting up signal catching */
	sigset_t sigset;
	RETSIGTYPE (*prev_sig_disp)(int);
	char errmsg[BUFSIZ];

	if (sigemptyset(&sigset)) {
		snprintf(errmsg, BUFSIZ, "Warning: couldn't initialise empty signal set");
		perror(errmsg);
	}
	for(sig_indx = 0; catch_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(catch_sig_list[sig_indx], sighandler)) == SIG_ERR) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't set signal %d for catching",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
		if(sigaddset(&sigset, catch_sig_list[sig_indx])) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't include signal %d for unblocking",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
	}
	for(sig_indx = 0; ignore_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(ignore_sig_list[sig_indx], SIG_IGN)) == SIG_ERR) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't set signal %d to ignore",
				ignore_sig_list[sig_indx]);
			perror(errmsg);
		}
	}

	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		snprintf(errmsg, BUFSIZ, "Warning: couldn't unblock signals");
		perror(errmsg);
	}
#endif
	
    /* purple initialization stuff
    */
	purple_util_set_user_dir(INI_STR("purple.custom_user_directory"));
    purple_debug_set_enabled(INI_INT("purple.debug_enabled"));
	purple_core_set_ui_ops(&php_core_uiops);
	purple_eventloop_set_ui_ops(&glib_eventloops);
	purple_plugins_add_search_path(INI_STR("purple.custom_plugin_path"));

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(purple)
{
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(purple)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(purple)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(purple)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "purple support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_loop)
{
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_prefs_load)
{
	RETURN_BOOL((long)purple_prefs_load());
}
/* }}} */


/*
**
**
** Purple account functions
**
*/

/* {{{ */
PHP_FUNCTION(purple_account_new)
{
	char *username, *protocol_name, *protocol_id;
	int username_len, protocol_name_len;
	GList *iter, *accounts;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &username, &username_len, &protocol_name, &protocol_name_len) == FAILURE) {
		RETURN_NULL();
	}
	
	iter = purple_plugins_get_protocols();
	for (; iter; iter = iter->next) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name && 0 == strcmp(info->name, protocol_name)) {
			protocol_id = estrdup(info->id);
		}
	}
//     php_printf("%s %s\n", username, protocol_id);
	account = purple_account_new(estrdup(username), estrdup(protocol_id));
	purple_accounts_add(account);
	if(NULL != account) {
		accounts = purple_accounts_get_all();
		RETURN_LONG((long)g_list_position(accounts, g_list_last(accounts)));
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_account_set_password)
{
	int account_index, password_len;
	char *password;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &account_index, &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}
	
	account = g_list_nth_data (purple_accounts_get_all(), (guint)account_index);
	if(NULL != account) {
		purple_account_set_password(account, estrdup(password));
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_account_set_enabled)
{
	int account_index, ui_id_len;
	char *ui_id;
	zend_bool enabled;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lsb", &account_index, &ui_id, &ui_id_len, &enabled) == FAILURE) {
		RETURN_NULL();
	}

	account = g_list_nth_data (purple_accounts_get_all(), (guint)account_index);
	if(NULL != account) {
		purple_account_set_enabled(account, estrdup(ui_id), (gboolean) enabled);
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_account_is_connected)
{
	int account_index;
	PurpleAccount *account = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &account_index) == FAILURE) {
		RETURN_NULL();
	}

	account = g_list_nth_data (purple_accounts_get_all(), (guint)account_index);
	if(NULL != account) {
		RETURN_BOOL(purple_account_is_connected(account));
	}

	RETURN_FALSE;
}
/* }}} */
/*
**
**
** End purple account functions
**
*/

/*
**
**
** Purple core functions
**
*/

/* {{{ */
PHP_FUNCTION(purple_core_get_version)
{
	char *version = estrdup(purple_core_get_version());
	
	RETURN_STRING(version, 0);
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_core_init)
{
	char *ui_id;
	int ui_id_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ui_id, &ui_id_len) == FAILURE) {
		RETURN_NULL();
	}
	
	ui_id = !ui_id_len ? INI_STR("purple.ui_id") : estrdup(ui_id);
	
	if (!purple_core_init(ui_id)) {
//         abort();
		RETURN_FALSE;
	}

	/* Load the preferences. */
	purple_prefs_load();

	/* Load the desired plugins. The client should save the list of loaded plugins in
	 * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
	purple_plugins_load_saved(INI_STR("purple.plugin_save_pref"));

	/* Load the pounces. */
	purple_pounces_load();
	
	RETURN_TRUE;
}
/* }}} */

/*
**
**
** End purple core functions
**
*/


/*
**
**
** Purple plugin functions
**
*/

/* {{{ */
PHP_FUNCTION(purple_plugins_get_protocols)
{
	GList *iter;
	int i;
	
	array_init(return_value);
	
	iter = purple_plugins_get_protocols();
	for (i = 0; iter; iter = iter->next, i++) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name) {
			add_index_string(return_value, i, info->name, 1);
		}
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_plugins_add_search_path)
{
	char *plugin_path;
	int plugin_path_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &plugin_path, &plugin_path_len) == FAILURE) {
		RETURN_NULL();
	}
	
	plugin_path = !plugin_path_len ? INI_STR("purple.custom_plugin_path") : estrdup(plugin_path);
	
	purple_plugins_add_search_path(plugin_path);
}
/* }}} */



/* {{{ */
PHP_FUNCTION(purple_plugins_load_saved)
{
	char* key;
	int key_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &key, &key_len) == FAILURE) {
		RETURN_NULL();
	}
	
	key = !key_len ? INI_STR("purple.plugin_save_pref") : estrdup(key);
	
	purple_plugins_load_saved(key);
}

/* }}} */

/*
**
**
** End purple plugin functions
**
*/


/*
**
**
** Purple signals functions
**
*/

PHP_FUNCTION(purple_signal_connect)
{
	static int handle;
	int signal_len;
	int ret = -1;
	char *signal;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &signal, &signal_len) == FAILURE) {
		RETURN_NULL();
	}

	if(0 == strcmp("signed-on", signal)) {
		ret = purple_signal_connect(	purple_connections_get_handle(),
										estrdup(signal),
										&handle,
										PURPLE_CALLBACK(purple_php_signed_on_function),
										NULL
									);
	}

	RETURN_LONG(ret);
}

/*
**
**
** End purple signals functions
**
*/

/*
**
**
** Purple status functions
**
*/

PHP_FUNCTION(purple_savedstatus_new)
{
	char *title;
	int primitive_status, title_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &title, &title_len, &primitive_status) == FAILURE) {
		RETURN_NULL();
	}

	saved_status = purple_savedstatus_new(title, primitive_status);
}


PHP_FUNCTION(purple_savedstatus_activate)
{
	purple_savedstatus_activate(saved_status);	
}

/*
**
**
** End purple status functions
**
*/

/*
**
**
** Purple util functions
**
*/

/* {{{ */
PHP_FUNCTION(purple_util_set_user_dir) {
	char *user_dir;
	int user_dir_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &user_dir, &user_dir_len) == FAILURE) {
		RETURN_NULL();
	}
	
	user_dir = !user_dir_len ? INI_STR("purple.custom_user_directory") : estrdup(user_dir);
	
	purple_util_set_user_dir(user_dir);
}
/* }}} */

/*
**
**
** End purple util functions
**
*/

/*
**
**
** Purple blist functions
**
*/

/* {{{ */
PHP_FUNCTION(purple_blist_load)
{
	purple_blist_load();
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_blist_new)
{
	purple_set_blist(purple_blist_new());
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_find_buddy)
{
	int account_index, name_len;
	char *name;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &account_index, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	account = g_list_nth_data (purple_accounts_get_all(), (guint)account_index);
	buddy = purple_find_buddy(account, name);
// 	php_printf("ba: %s\n", 	purple_buddy_get_alias_only(buddy));
}
/* }}} */
/*
**
**
** End purple blist functions
**
*/

/*
**
**
** Purple pounce functions
**
*/

PHP_FUNCTION(purple_pounces_load)
{
	RETURN_BOOL(purple_pounces_load());
}

/*
**
**
** End purple pounce functions
**
*/

/*
**
**
** Purple conversation functions
**
*/

/* {{{ */
PHP_FUNCTION(purple_php_write_conv_function)
{
	zval *arg;
	char *name;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &arg)) {
		RETURN_FALSE;
	}

	if (!zend_is_callable(arg, 0, &name)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s is not callable", name);
		efree(name);
		RETURN_FALSE;
	}

	purple_php_write_conv_function_name = purple_php_string_zval(name);
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_conversation_get_name)
{
	int conversation_index;
	PurpleConversation *conversation = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &conversation_index) == FAILURE) {
		RETURN_NULL();
	}

	conversation = g_list_nth_data (purple_get_conversations(), (guint)conversation_index);

	if(NULL != conversation) {
		RETURN_STRING(purple_conversation_get_name(conversation), 1);
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_conversation_new)
{
	int type, account_index, name_len;
	char *name;
	PurpleConversation *conv = NULL;
	PurpleAccount *account = NULL;
	GList *conversations = purple_get_conversations();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lls", &type, &account_index, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

 	account = g_list_nth_data (purple_accounts_get_all(), (guint)account_index);
	if(NULL != account) {
		conv = purple_conversation_new(type, account, estrdup(name));
		RETURN_LONG((long)g_list_position(conversations, g_list_last(conversations)));
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_conversation_write)
{
	int conversation_index, flags, mtime, who_len, message_len;
	char *who, *message;
	PurpleConversation *conversation = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lssll", &conversation_index, &who, &who_len, &message, &message_len, &flags, &mtime) == FAILURE) {
		RETURN_NULL();
	}
	
	conversation = g_list_nth_data (purple_get_conversations(), (guint)conversation_index);
	if(NULL != conversation) {
// 		purple_conv_im_send (PURPLE_CONV_IM(conversation), estrdup("hello you fuck!\n"));
		purple_conversation_write(conversation, estrdup(who), estrdup(message), flags, (time_t)mtime);
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_conv_im_send)
{
	int conversation_index, message_len;
	char *message;
	PurpleConversation *conversation = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &conversation_index, &message, &message_len) == FAILURE) {
		RETURN_NULL();
	}

	conversation = g_list_nth_data (purple_get_conversations(), (guint)conversation_index);
	if(NULL != conversation) {
		purple_conv_im_send (PURPLE_CONV_IM(conversation), estrdup(message));
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_conversation_set_account)
{
	int conversation_index, account_index;
	PurpleConversation *conversation = NULL;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &conversation_index, &account_index) == FAILURE) {
		RETURN_NULL();
	}

	conversation = g_list_nth_data (purple_get_conversations(), (guint)conversation_index);
	account = g_list_nth_data (purple_accounts_get_all(), (guint)account_index);
	if(NULL != conversation && NULL != account) {
		purple_conversation_set_account(conversation, account);
	}
}
/* }}} */


/* {{{ */
/*PHP_FUNCTION(purple_conv_im_write)
{
	
}*/
/* }}} */
/*
**
**
** End purple conversation functions
**
*/

/*
**
** Helper functions
**
*/

static void
php_ui_init()
{
	purple_conversations_set_ui_ops(&php_conv_uiops);
}

#ifdef HAVE_SIGNAL_H
static void
clean_pid()
{
	int status;
	pid_t pid;

	do {
		pid = waitpid(-1, &status, WNOHANG);
	} while (pid != 0 && pid != (pid_t)-1);

	if ((pid == (pid_t) - 1) && (errno != ECHILD)) {
		char errmsg[BUFSIZ];
		snprintf(errmsg, BUFSIZ, "Warning: waitpid() returned %d", pid);
		perror(errmsg);
	}

	/* Restore signal catching */
	signal(SIGALRM, sighandler);
}

static void
sighandler(int sig)
{
	switch (sig) {
	case SIGHUP:
		purple_debug_warning("sighandler", "Caught signal %d\n", sig);
		purple_connections_disconnect_all();
		break;
	case SIGSEGV:
		fprintf(stderr, "%s", segfault_message);
		abort();
		break;
	case SIGCHLD:
		/* Restore signal catching */
		signal(SIGCHLD, sighandler);
		alarm(1);
		break;
	case SIGALRM:
		clean_pid();
		break;
	default:
		purple_debug_warning("sighandler", "Caught signal %d\n", sig);
		purple_connections_disconnect_all();

		purple_plugins_unload_all();

		exit(0);
	}
}
#endif


/* {{{ just took this two functions from the readline extension */
static zval *purple_php_string_zval(const char *str)
{
	zval *ret;
	int len;
	
	MAKE_STD_ZVAL(ret);
	
	if (str) {
		len = strlen(str);
		ZVAL_STRINGL(ret, (char*)str, len, 1);
	} else {
		ZVAL_NULL(ret);
	}

	return ret;
}

static zval *purple_php_long_zval(long l)
{
	zval *ret;
	MAKE_STD_ZVAL(ret);

	Z_TYPE_P(ret) = IS_LONG;
	Z_LVAL_P(ret) = l;
	return ret;
}
/* }}} */

static void purple_glib_io_destroy(gpointer data) 
{
	g_free(data); 
}


static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleGLibIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;
	
	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;
	
	closure->function(closure->data, g_io_channel_unix_get_fd(source),
				purple_cond);
	
	return TRUE;
}


static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
                               gpointer data)
{
	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;
	
	closure->function = function;
	closure->data = data;
	
	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;
	
	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
							purple_glib_io_invoke, closure, purple_glib_io_destroy);
	
	g_io_channel_unref(channel);
	return closure->result;
}


/* {{{ */
static void
purple_php_write_conv_function(PurpleConversation *conv, const char *who, const char *alias,
            const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const int PARAMS_COUNT = 6;
	zval *params[PARAMS_COUNT];
	zval *retval;
	GList *conversations = purple_get_conversations();
	
	TSRMLS_FETCH();

	params[0] = purple_php_long_zval((long)g_list_position(conversations, g_list_last(conversations)));
	params[1] = purple_php_string_zval(who);
	params[2] = purple_php_string_zval(alias);
	params[3] = purple_php_string_zval(message);
	params[4] = purple_php_long_zval((long)flags);
	params[5] = purple_php_long_zval((long)mtime);

	if(call_user_function(	CG(function_table),
	   						NULL,
							purple_php_write_conv_function_name,
							retval,
							PARAMS_COUNT,
							params TSRMLS_CC
						 ) != SUCCESS) {
		zend_error(E_ERROR, "Function call failed");
	}

	zval_dtor(retval);
}
/* }}} */

/*
static void php_write_chat(PurpleConversation *conv, const char *who, const char *message,
						   PurpleMessageFlags flags, time_t mtime)
{
	php_printf("%s + write_chat\n", message);
}

static void php_write_im(PurpleConversation *conv, const char *who, const char *message,
						 PurpleMessageFlags flags, time_t mtime)
{
	php_printf("%s + write_im\n", message);
	
// 	purple_conv_im_send(purple_conversation_get_im_data(conv), "answer: you are an asshole ;) !!! \n");
// 	purple_conv_im_write(purple_conversation_get_im_data(conv), who, "answer: you are an asshole ;) !!! \n", flags, mtime);
// 	common_send(conv, g_strdup("answer: you are an asshole ;) !!! \n"), flags);
}*/

static void
purple_php_signed_on_function(PurpleConnection *gc, gpointer null)
{/*
	PurpleAccount *account = purple_connection_get_account(gc);
	php_printf("Account connected: %s %s\n", account->username, account->protocol_id);   */
}


/*
**
** End helper functions
**
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
