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
#include <pcre.h>
 
#include <string.h>
#include <ctype.h>

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

static void purple_php_ui_init();
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
zval* call_user_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, zval* params[]);
zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... TSRMLS_DC);
void purple_php_dump_zval(zval *var);
char *purple_php_tolower(const char *s);
char *purple_php_get_protocol_id_by_name(const char *name);
#ifdef HAVE_SIGNAL_H
static void sighandler(int sig);
static void clean_pid();
#endif


#ifdef HAVE_SIGNAL_H
static char *segfault_message = "";

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
	purple_php_ui_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

ZEND_DECLARE_MODULE_GLOBALS(purple);

void globals_ctor(zend_purple_globals *purple_globals TSRMLS_DC)
{
	ALLOC_INIT_ZVAL(purple_globals->purple_php_client_obj);
	Z_TYPE_P(purple_globals->purple_php_client_obj) = IS_OBJECT;

}
void globals_dtor(zend_purple_globals *purple_globals TSRMLS_DC) { }

/* True global resources - no need for thread safety here */
static int le_purple;


/* classes definitions*/
static zend_class_entry *Client_ce, *Conversation_ce, *Account_ce, *Connection_ce;


/* {{{ purple_functions[]
 *
 * Every user visible function must have an entry in purple_functions[].
 */
zend_function_entry purple_functions[] = {
	{NULL, NULL, NULL}	/* Must be the last line in purple_functions[] */
};
/* }}} */

/* client class methods */
zend_function_entry Client_methods[] = {
	PHP_ME(Client, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
	PHP_ME(Client, getInstance, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(Client, initInternal, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(Client, getCoreVersion, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Client, initCore, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Client, connectToSignal, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Client, writeConv, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(Client, onSignedOn, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(Client, runLoop, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Client, addAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Client, getProtocols, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(Client, setUserDir, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};


/* conversation class methods*/
zend_function_entry Conversation_methods[] = {
	PHP_ME(Conversation, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Conversation, getName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Conversation, sendIM, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};


/* account class methods*/
zend_function_entry Account_methods[] = {
	PHP_ME(Account, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Account, setPassword, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Account, setEnabled, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/* connection class methods*/
zend_function_entry Connection_methods[] = {
	PHP_ME(Connection, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Connection, getAccount, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};


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
	STD_PHP_INI_ENTRY("purple.custom_user_directory", "/dev/null", PHP_INI_ALL, OnUpdateString, custom_user_directory, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.custom_plugin_path", "", PHP_INI_ALL, OnUpdateString, custom_plugin_path, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.ui_id", "php", PHP_INI_ALL, OnUpdateString, ui_id, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.debug_enabled", "0", PHP_INI_ALL, OnUpdateLong, debug_enabled, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.plugin_save_pref", "/purple/nullclient/plugins/saved", PHP_INI_ALL, OnUpdateString, plugin_save_pref, zend_purple_globals, purple_globals)
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

	/* initalizing classes */
	zend_class_entry ce;
	
	/* classes definitions */

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Client", Client_methods);
#else
	INIT_CLASS_ENTRY(ce, "Client", Client_methods);
#endif
	Client_ce = zend_register_internal_class(&ce TSRMLS_DC);
	/*zend_declare_property_string(Client_ce, "ui_id", sizeof("ui_id")-1, INI_STR("purple.ui_id"), ZEND_ACC_PRIVATE TSRMLS_DC);*/

	/* A type of conversation */
	zend_declare_class_constant_long(Client_ce, "CONV_TYPE_UNKNOWN", sizeof("CONV_TYPE_UNKNOWN")-1, PURPLE_CONV_TYPE_UNKNOWN TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "CONV_TYPE_IM", sizeof("CONV_TYPE_IM")-1, PURPLE_CONV_TYPE_IM TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "CONV_TYPE_CHAT", sizeof("CONV_TYPE_CHAT")-1, PURPLE_CONV_TYPE_CHAT TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "CONV_TYPE_MISC", sizeof("CONV_TYPE_MISC")-1, PURPLE_CONV_TYPE_MISC TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "CONV_TYPE_ANY", sizeof("CONV_TYPE_ANY")-1, PURPLE_CONV_TYPE_ANY TSRMLS_DC);
	/* Flags applicable to a message */
	zend_declare_class_constant_long(Client_ce, "MESSAGE_SEND", sizeof("MESSAGE_SEND")-1, PURPLE_MESSAGE_SEND TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_RECV", sizeof("MESSAGE_RECV")-1, PURPLE_MESSAGE_RECV TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_SYSTEM", sizeof("MESSAGE_SYSTEM")-1, PURPLE_MESSAGE_SYSTEM TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_AUTO_RESP", sizeof("MESSAGE_AUTO_RESP")-1, PURPLE_MESSAGE_AUTO_RESP TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_ACTIVE_ONLY", sizeof("MESSAGE_ACTIVE_ONLY")-1, PURPLE_MESSAGE_ACTIVE_ONLY TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_NICK", sizeof("MESSAGE_NICK")-1, PURPLE_MESSAGE_NICK TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_NO_LOG", sizeof("MESSAGE_NO_LOG")-1, PURPLE_MESSAGE_NO_LOG TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_WHISPER", sizeof("MESSAGE_WHISPER")-1, PURPLE_MESSAGE_WHISPER TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_ERROR", sizeof("MESSAGE_ERROR")-1, PURPLE_MESSAGE_ERROR TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_DELAYED", sizeof("MESSAGE_DELAYED")-1, PURPLE_MESSAGE_DELAYED TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_RAW", sizeof("MESSAGE_RAW")-1, PURPLE_MESSAGE_RAW TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_IMAGES", sizeof("MESSAGE_IMAGES")-1, PURPLE_MESSAGE_IMAGES TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_NOTIFY", sizeof("MESSAGE_NOTIFY")-1, PURPLE_MESSAGE_NOTIFY TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_NO_LINKIFY", sizeof("MESSAGE_NO_LINKIFY")-1, PURPLE_MESSAGE_NO_LINKIFY TSRMLS_DC);
	zend_declare_class_constant_long(Client_ce, "MESSAGE_INVISIBLE", sizeof("MESSAGE_INVISIBLE")-1, PURPLE_MESSAGE_INVISIBLE TSRMLS_DC);
	
#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Conversation", Conversation_methods);
#else
	INIT_CLASS_ENTRY(ce, "Conversation", Conversation_methods);
#endif
	Conversation_ce = zend_register_internal_class(&ce TSRMLS_DC);
	zend_declare_property_long(Conversation_ce, "index", sizeof("index")-1, -1, ZEND_ACC_PRIVATE TSRMLS_DC);

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Account", Account_methods);
#else
	INIT_CLASS_ENTRY(ce, "Account", Account_methods);
#endif
	Account_ce = zend_register_internal_class(&ce TSRMLS_DC);
	zend_declare_property_long(Account_ce, "index", sizeof("index")-1, -1, ZEND_ACC_PRIVATE TSRMLS_DC);
	
#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Connection", Connection_methods);
#else
	INIT_CLASS_ENTRY(ce, "Connection", Connection_methods);
#endif
	Connection_ce = zend_register_internal_class(&ce TSRMLS_DC);
	zend_declare_property_long(Connection_ce, "index", sizeof("index")-1, -1, ZEND_ACC_PRIVATE TSRMLS_DC);

	/* end initalizing classes */
	
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

    /*
	 * purple initialization stuff
	 */
	purple_util_set_user_dir(INI_STR("purple.custom_user_directory"));
    purple_debug_set_enabled(INI_INT("purple.debug_enabled"));
	purple_core_set_ui_ops(&php_core_uiops);
	purple_eventloop_set_ui_ops(&glib_eventloops);
	purple_plugins_add_search_path(INI_STR("purple.custom_plugin_path"));

	if (!purple_core_init((const char *) estrdup(INI_STR("purple.ui_id")))) {
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif
		return FAILURE;
	}

	purple_set_blist(purple_blist_new());
	purple_blist_load();
	purple_prefs_load();

	purple_plugins_load_saved(INI_STR("purple.plugin_save_pref"));

	PurpleSavedStatus *saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
	purple_savedstatus_activate(saved_status);

	/* init globals */
	ZEND_INIT_MODULE_GLOBALS(purple, globals_ctor, globals_dtor);
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(purple)
{
	UNREGISTER_INI_ENTRIES();

#ifdef ZTS
	ts_free_id(purple_globals_id);
#else
	globals_dtor(&purple_globals TSRMLS_CC);
#endif
	
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
/*	php_info_print_table_start();
	php_info_print_table_header(2, "purple support", "enabled");
	php_info_print_table_end();
*/
	
	DISPLAY_INI_ENTRIES();

}
/* }}} */


/*
**
**
** Purple client methods
**
*/


/* {{{ proto int connectToSignal(string signal)
	Connects a signal handler to a signal for a particular object */
PHP_METHOD(Client, connectToSignal)
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
/* }}} */


/* {{{ proto void Client::loop(void)
	Creates the main loop*/
PHP_METHOD(Client, runLoop)
{
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
}
/* }}} */


/* {{{ proto bool purple_prefs_load(void)
	Loads the user preferences from the user dir*/

/* }}} */


/* {{{ proto object Client::addAccount(string dsn)*/
PHP_METHOD(Client, addAccount)
{
	char *account_dsn, *protocol, *nick, *password;
	const char *error;
	int account_dsn_len, erroffset, offsets[12], rc;
	pcre *re;
	PurpleAccount *account = NULL;
	GList *accounts;
	zval *ui_id;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &account_dsn, &account_dsn_len) == FAILURE) {
		RETURN_FALSE;
	}

	re = pcre_compile("(.+)://(.+):(.+)", 0, &error, &erroffset, NULL);

	if (re == NULL)
	{
		zend_throw_exception(NULL, "PCRE compilation failed at offset %d: %s", erroffset, error, 0 TSRMLS_CC);
		return;
	}

	rc = pcre_exec(re, NULL, account_dsn, account_dsn_len, 0, 0, offsets, 12);

	if(rc < 0)
	{
		switch(rc)
			{
			case PCRE_ERROR_NOMATCH:
				zend_throw_exception(NULL, "The account string must match \"protocol://user:password\" pattern", 0 TSRMLS_CC);
			break;

			default:
				zend_throw_exception(NULL, "The account string must match \"protocol://user:password pattern\". Matching error %d", rc, 0 TSRMLS_CC);
			break;
			}
		pcre_free(re);
		return;
	}

	protocol = emalloc(offsets[3] - offsets[2] + 1);
	php_sprintf(protocol, "%.*s", offsets[3] - offsets[2], account_dsn + offsets[2]);
	nick = emalloc(offsets[5] - offsets[4] + 1);
	php_sprintf(nick, "%.*s", offsets[5] - offsets[4], account_dsn + offsets[4]);
	password = emalloc(offsets[7] - offsets[6] + 1);
	php_sprintf(password, "%.*s", offsets[7] - offsets[6], account_dsn + offsets[6]);

	account = purple_account_new(estrdup(nick), purple_php_get_protocol_id_by_name(protocol));

	if(NULL != account) {

		purple_account_set_password(account, estrdup(password));

		ui_id = zend_read_property(Z_OBJCE_P(getThis()), getThis(), "ui_id", sizeof("ui_id")-1, 0 TSRMLS_DC);
		purple_account_set_enabled(account, Z_STRVAL_P(ui_id), 1);

		purple_accounts_add(account);

		accounts = purple_accounts_get_all();

		MAKE_STD_ZVAL(return_value);
		Z_TYPE_P(return_value) = IS_OBJECT;
		object_init_ex(return_value, Account_ce);
		zend_update_property_long(	Account_ce,
									return_value,
									"index",
									sizeof("index")-1,
									(long)g_list_position(accounts, g_list_find(accounts, account))
									);

		efree(protocol);
		efree(nick);
		efree(password);

		return;

	}

		efree(protocol);
		efree(nick);
		efree(password);

		RETURN_NULL();
}
/* }}} */


/* {{{ proto string Purple::getCoreVersion(void)
	Returns the libpurple core version string */
PHP_METHOD(Client, getCoreVersion)
{	
	char *version = estrdup(purple_core_get_version());

	RETURN_STRING(version, 0);
}
/* }}} */


/* {{{ proto bool Purple::initCore([string ui_id])
	Initalizes the libpurple core */
PHP_METHOD(Client, initCore)
{
	char *ui_id;
	int ui_id_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ui_id, &ui_id_len) == FAILURE) {
		RETURN_NULL();
	}

	ui_id = !ui_id_len ? INI_STR("purple.ui_id") : estrdup(ui_id);

	if (!purple_core_init(ui_id)) {
#ifdef HAVE_SIGNAL_H
		g_free(segfault_message);
#endif
		RETURN_FALSE;
	}

	purple_set_blist(purple_blist_new());
	purple_blist_load();
	
	/* Load the preferences. */
	purple_prefs_load();

	/* Load the desired plugins. The client should save the list of loaded plugins in
	 * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
	purple_plugins_load_saved(INI_STR("purple.plugin_save_pref"));

	/* Load the pounces. */
	/*purple_pounces_load();*/

	PurpleSavedStatus *saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
	purple_savedstatus_activate(saved_status);

	RETURN_TRUE;
}
/* }}} */

PHP_METHOD(Client, __construct)
{

}


PHP_METHOD(Client, getInstance)
{
	zval *tmp;
	
	if(NULL == zend_objects_get_address(PURPLE_G(purple_php_client_obj))) {
		MAKE_STD_ZVAL(tmp);
		Z_TYPE_P(tmp) = IS_OBJECT;
#if ZEND_MODULE_API_NO >= 20071006
		object_init_ex(tmp, EG(called_scope));
#else
		zend_class_entry **ce = NULL;
		zend_hash_find(EG(class_table), "purpleclient", sizeof("purpleclient"), (void **) &ce);

		if(ce && (*ce)->parent && 0 == strcmp("Client", (*ce)->parent->name)) {
			object_init_ex(tmp, *ce);
		} else {
			zend_throw_exception(NULL, "The Client child class must be named PurpleClient for php < 5.3", 0 TSRMLS_CC);
			return;
		}
		/* object_init_ex(tmp, EG(current_execute_data->fbc->common.scope)); would be beautiful but works not as expected */
		
#endif

		zend_update_property_string(	Z_OBJCE_P(tmp),
										tmp,
										"ui_id",
										sizeof("ui_id")-1,
										INI_STR("purple.ui_id") TSRMLS_DC
										);
		
		*PURPLE_G(purple_php_client_obj) = *tmp;
		zval_copy_ctor(PURPLE_G(purple_php_client_obj));

		*return_value = *tmp;
		zval_copy_ctor(return_value);

		call_custom_method(	&PURPLE_G(purple_php_client_obj),
							Z_OBJCE_P(PURPLE_G(purple_php_client_obj)),
							NULL,
							"initinternal",
							sizeof("initinternal")-1,
							NULL,
							0,
							NULL TSRMLS_DC);
		
		efree(tmp);
		
		return;
	}

	*return_value = *PURPLE_G(purple_php_client_obj);
	zval_copy_ctor(return_value);
	
	return;
}

/* {{{ proto array Client::getProtocols(void)
	Returns a list of all valid protocol plugins */
PHP_METHOD(Client, getProtocols)
{
	GList *iter = purple_plugins_get_protocols();
	zval *protocols;
	
	MAKE_STD_ZVAL(protocols);
	array_init(protocols);
	
	for (; iter; iter = iter->next) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name) {
			add_next_index_string(protocols, info->name, 1);
		}
	}
	
	*return_value = *protocols;
	zval_copy_ctor(return_value);

	efree(protocols);
	g_list_free(iter);
	
	return;
}
/* }}} */

/* {{{ */
PHP_METHOD(Client, initInternal)
{

}
/* }}}*/

/* {{{ proto void Client::setUserDir([string $userDir])
	Define a custom purple settings directory, overriding the default (user's home directory/.purple) */
PHP_METHOD(Client, setUserDir) {
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
** End purple client methods
**
*/


/*
**
**
** Purple client callback methods
**
*/

PHP_METHOD(Client, writeConv)
{

}

PHP_METHOD(Client, onSignedOn)
{

}

/*
**
**
** End purple client callback methods
**
*/


/*
**
**
** Purple connection methods
**
*/

PHP_METHOD(Connection, __construct)
{
	
}

/* {{{ proto Account Connection::getAccount()
		Returns the connection's account*/
PHP_METHOD(Connection, getAccount)
{
	PurpleConnection *conn = NULL;
	PurpleAccount *acc = NULL;
	GList *accounts = NULL;

	conn = g_list_nth_data (purple_connections_get_all(), (guint)Z_LVAL_P(zend_read_property(Connection_ce, getThis(), "index", sizeof("index")-1, 0)));
	if(NULL != conn) {
		acc = purple_connection_get_account(conn);
		if(NULL != acc) {
			accounts = purple_accounts_get_all();

			ZVAL_NULL(return_value);
			Z_TYPE_P(return_value) = IS_OBJECT;
			object_init_ex(return_value, Account_ce);
			zend_update_property_long(	Account_ce,
										return_value,
										"index",
										sizeof("index")-1,
										(long)g_list_position(accounts, g_list_find(accounts, acc))
										);
			return;
		}
	}

	RETURN_NULL();
}
/* }}} */


/*
**
**
** End purple connection methods
**
*/


/*
**
**
** Purple account methods
**
*/

/* {{{ proto Object __construct(string user_name, string protocol_name)
	Creates a new account*/
PHP_METHOD(Account, __construct)
{
	char *username, *protocol_name, *protocol_id;
	int username_len, protocol_name_len;
	GList *iter, *accounts;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &username, &username_len, &protocol_name, &protocol_name_len) == FAILURE) {
		RETURN_NULL();
	}

	account = purple_account_new(estrdup(username), purple_php_get_protocol_id_by_name(protocol_name));
	purple_accounts_add(account);
	if(NULL != account) {
		accounts = purple_accounts_get_all();

		zend_update_property_long(	Account_ce,
									getThis(),
									"index",
									sizeof("index")-1,
									(long)g_list_position(accounts, g_list_last(accounts))
								 );
		return;
		
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto void setPassword(int account, string password)
	Sets the account's password */
PHP_METHOD(Account, setPassword)
{
	int password_len;
	char *password;
	PurpleAccount *account = NULL;
	zval *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(Account_ce, getThis(), "index", sizeof("index")-1, 0)));

	/*php_printf("account_index = %d\n", Z_LVAL_P(account_index));*/

 	account = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != account) {
		purple_account_set_password(account, estrdup(password));
	}
}
/* }}} */


/* {{{ proto void purple_account_set_enabled(int account, string ui_id, bool enabled)
	Sets whether or not this account is enabled for the specified UI */
PHP_METHOD(Account, setEnabled)
{
	int ui_id_len;
	char *ui_id;
	zend_bool enabled;
	PurpleAccount *account = NULL;
	zval *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sb", &ui_id, &ui_id_len, &enabled) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(Account_ce, getThis(), "index", sizeof("index")-1, 0)));
	
	account = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != account) {
		purple_account_set_enabled(account, estrdup(ui_id), (gboolean) enabled);
	}
}
/* }}} */


/* {{{ proto bool purple_account_is_connected(int account)
	Returns whether or not the account is connected*/
/* }}} */
/*
**
**
** End purple account methods
**
*/

/*
**
**
** Purple util methods
**
*/

/* {{{ proto void purple_util_set_user_dir([string user_dir])
	Define a custom purple settings directory, overriding the default (user's home directory/.purple) */
/* }}} */

/*
**
**
** End purple util methods
**
*/

/*
**
**
** Purple blist methods
**
*/

/* {{{ proto void purple_blist_load(void)
		Loads the buddy list from ~/.purple/blist.xml */
/* }}} */


/* {{{ */
/* }}} */


/* {{{ */
/* }}} */
/*
**
**
** End purple blist methods
**
*/

/*
**
**
** Purple pounce methods
**
*/

/*
**
**
** End purple pounce methods
**
*/

/*
**
**
** Purple conversation methods
**
*/

/* {{{ proto int Conversation::__construct(int type, Object account, string name)
	Creates a new conversation of the specified type */
PHP_METHOD(Conversation, __construct)
{
	int type, name_len;
	char *name;
	PurpleConversation *conv = NULL;
	PurpleAccount *paccount = NULL;
	GList *conversations = NULL;
	zval *account, *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "los", &type, &account, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	account_index = zend_read_property(Account_ce, account, "index", sizeof("index")-1, 0);
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));

	if(NULL != account) {
		conv = purple_conversation_new(type, paccount, estrdup(name));
		conversations = purple_get_conversations();

		zend_update_property_long(	Conversation_ce,
									getThis(),
									"index",
									sizeof("index")-1,
									(long)g_list_position(conversations, g_list_last(conversations))
									);
		return;
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto string Conversation::getName(void)
	Returns the specified conversation's name*/
PHP_METHOD(Conversation, getName)
{
	zval *conversation_index;
	PurpleConversation *conversation = NULL;

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(Conversation_ce, getThis(), "index", sizeof("index")-1, 0)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		RETURN_STRING(estrdup(purple_conversation_get_name(conversation)), 1);
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto void sendIM(string message)
	Sends a message to this IM conversation */
PHP_METHOD(Conversation, sendIM)
{
	int message_len;
	char *message;
	PurpleConversation *conversation = NULL;
	zval *conversation_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &message, &message_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(Conversation_ce, getThis(), "index", sizeof("index")-1, 0)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		purple_conv_im_send (PURPLE_CONV_IM(conversation), estrdup(message));
	}
}
/* }}} */


/* {{{ proto void purple_conversation_set_account(int conversation, int account)
	Sets the specified conversation's purple_account */
/* }}} */


/* {{{ */
/*PHP_FUNCTION(purple_conv_im_write)
{
	
}*/
/* }}} */
/*
**
**
** End purple conversation methods
**
*/



/*
**
** Helper functions
**
*/

static void
purple_php_ui_init()
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
	zval ***params, *conversation, *retval, *tmp1, *tmp2, *tmp3, *tmp4, *tmp5;
	GList *conversations = purple_get_conversations();
	zend_fcall_info fci;
	zend_fcall_info_cache fcic;
	
	TSRMLS_FETCH();

	MAKE_STD_ZVAL(conversation);
	Z_TYPE_P(conversation) = IS_OBJECT;
	object_init_ex(conversation, Conversation_ce);
	zend_update_property_long(	Conversation_ce,
								conversation,
								"index",
								sizeof("index")-1,
								(long)g_list_position(conversations, g_list_find(conversations, conv))
								);
/*
	MAKE_STD_ZVAL(datetime);
	Z_TYPE_P(datetime) = IS_OBJECT;

	object_and_properties_init(datetime, DateTime, );*/

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	HashTable *function_table = &ce->function_table;

    tmp1 = purple_php_string_zval(who);
    tmp2 = purple_php_string_zval(alias);
    tmp3 = purple_php_string_zval(message);
    tmp4 = purple_php_long_zval((long)flags);
    tmp5 = purple_php_long_zval((long)mtime);
	
	call_custom_method(	&client,
						ce,
	  					NULL,
						"writeconv",
	  					sizeof("writeconv")-1,
						NULL,
	  					PARAMS_COUNT,
						&conversation,
					  	&tmp1,
						&tmp2,
	  					&tmp3,
						&tmp4,
	  					&tmp5 TSRMLS_DC
					  );

	zval_ptr_dtor(&tmp1);
	zval_ptr_dtor(&tmp2);
	zval_ptr_dtor(&tmp3);
	zval_ptr_dtor(&tmp4);
	zval_ptr_dtor(&tmp5);

	
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
	
	purple_conv_im_send(purple_conversation_get_im_data(conv), "answer: you are an asshole ;) !!! \n");
	purple_conv_im_write(purple_conversation_get_im_data(conv), who, "answer: you are an asshole ;) !!! \n", flags, mtime);
	common_send(conv, g_strdup("answer: you are an asshole ;) !!! \n"), flags);
}*/

static void
purple_php_signed_on_function(PurpleConnection *conn, gpointer null)
{
	zval *connection, *retval;
	GList *connections = NULL;
	
	TSRMLS_FETCH();

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	HashTable *function_table = &ce->function_table;
	
	connections = purple_connections_get_all();

	MAKE_STD_ZVAL(connection);
	Z_TYPE_P(connection) = IS_OBJECT;
	object_init_ex(connection, Connection_ce);
	zend_update_property_long(	Connection_ce,
								connection,
								"index",
								sizeof("index")-1,
								(long)g_list_position(connections, g_list_find(connections, conn))
								);

	call_custom_method(	&client,
						ce,
	  					NULL,
						"onsignedon",
	  					sizeof("onsignedon")-1,
						NULL,
	  					1,
						&connection TSRMLS_DC);
	
	zval_ptr_dtor(&connection);
}

/*
**
** End helper functions
**
*/


/* {{{
 Only returns the returned zval if retval_ptr != NULL */
zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... TSRMLS_DC)
{
	int result, i;
	zend_fcall_info fci;
	zval z_fname;
	zval *retval, *null, **tmp;
	HashTable *function_table;

	va_list given_params;
	zval ***params;
	params = (zval ***) safe_emalloc(param_count, sizeof(zval **), 0);
	va_start(given_params, param_count);
	for(i=0;i<param_count;i++) {
		params[i] = va_arg(given_params, zval **);
// 		purple_php_dump_zval(*params[i]);
	}
	va_end(given_params);
	
	fci.size = sizeof(fci);
	/*fci.function_table = NULL; will be read form zend_class_entry of object if needed */
	fci.object_pp = object_pp;
	fci.function_name = &z_fname;
	fci.retval_ptr_ptr = retval_ptr_ptr ? retval_ptr_ptr : &retval;
	fci.param_count = param_count;
	fci.params = params;
	fci.no_separation = 1;
	fci.symbol_table = NULL;

	if (!fn_proxy && !obj_ce) {
		/* no interest in caching and no information already present that is
		 * needed later inside zend_call_function. */
		ZVAL_STRINGL(&z_fname, function_name, function_name_len, 0);
		fci.function_table = !object_pp ? EG(function_table) : NULL;
		result = zend_call_function(&fci, NULL TSRMLS_CC);
	} else {
		zend_fcall_info_cache fcic;

		fcic.initialized = 1;
		if (!obj_ce) {
			obj_ce = object_pp ? Z_OBJCE_PP(object_pp) : NULL;
		}
		if (obj_ce) {
			function_table = &obj_ce->function_table;
		} else {
			function_table = EG(function_table);
		}
		if (!fn_proxy || !*fn_proxy) {
			if (zend_hash_find(function_table, function_name, function_name_len+1, (void **) &fcic.function_handler) == FAILURE) {
				/* error at c-level */
				zend_error(E_CORE_ERROR, "Couldn't find implementation for method %s%s%s", obj_ce ? obj_ce->name : "", obj_ce ? "::" : "", function_name);
			}
			if (fn_proxy) {
				*fn_proxy = fcic.function_handler;
			}
		} else {
			fcic.function_handler = *fn_proxy;
		}
		fcic.calling_scope = obj_ce;
		fcic.object_pp = object_pp;
		result = zend_call_function(&fci, &fcic TSRMLS_CC);
	}
	if (result == FAILURE) {
		/* error at c-level */
		if (!obj_ce) {
			obj_ce = object_pp ? Z_OBJCE_PP(object_pp) : NULL;
		}
		if (!EG(exception)) {
			zend_error(E_CORE_ERROR, "Couldn't execute method %s%s%s", obj_ce ? obj_ce->name : "", obj_ce ? "::" : "", function_name);
		}
	}
	if(params) {
		efree(params);
	}
	if (!retval_ptr_ptr) {
		if (retval) {
			zval_ptr_dtor(&retval);
		}
		return NULL;
	}
	return *retval_ptr_ptr;
}
/* }}} */

void purple_php_dump_zval(zval *var)
{

    switch (Z_TYPE_P(var)) {
        case IS_NULL:
            php_printf("NULL ");
            break;
        case IS_BOOL:
            php_printf("Boolean: %s ", Z_LVAL_P(var) ? "TRUE" : "FALSE");
            break;
        case IS_LONG:
            php_printf("Long: %ld ", Z_LVAL_P(var));
            break;
        case IS_DOUBLE:
            php_printf("Double: %f ", Z_DVAL_P(var));
            break;
        case IS_STRING:
            php_printf("String: ");
            PHPWRITE(Z_STRVAL_P(var), Z_STRLEN_P(var));
            php_printf(" ");
            break;
        case IS_RESOURCE:
            php_printf("Resource ");
            break;
        case IS_ARRAY:
            php_printf("Array ");
            break;
        case IS_OBJECT:
            php_printf("Object ");
            break;
        default:
            php_printf("Unknown ");
    }
}

/* {{{ */
char *purple_php_tolower(const char *s)
{
	int  i = 0;
	char *r = estrdup(s);

	while (r[i])
	{
		r[i] = tolower(r[i]);
		i++;
	}

	return r;
}
/* }}} */


/* {{{ */
char *purple_php_get_protocol_id_by_name(const char *protocol_name)
{
	GList *iter;

	iter = purple_plugins_get_protocols();

	for (; iter; iter = iter->next) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name && 0 == strcmp(purple_php_tolower(info->name), purple_php_tolower(protocol_name))) {
			return estrdup(info->id);
		}
	}

	return "";
}
/* }}} */



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
