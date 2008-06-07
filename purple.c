/**
 * Copyright (c) 2007-2008, Anatoliy Belsky
 *
 * This file is part of PHPurple.
 *
 * PHPurple is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHPurple is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHPurple.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include <pcre/pcrelib/pcre.h>

#include "php_purple.h"

#include <glib.h>

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
#include "request.h"

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
static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function, gpointer data);
static void purple_php_write_conv_function(PurpleConversation *conv, const char *who, const char *alias, const char *message, PurpleMessageFlags flags, time_t mtime);
static void purple_php_write_im_function(PurpleConversation *conv, const char *who, const char *message, PurpleMessageFlags flags, time_t mtime);
static void purple_php_signed_on_function(PurpleConnection *gc, gpointer null);
static zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... );
static char *purple_php_tolower(const char *s);
static char *purple_php_get_protocol_id_by_name(const char *name);
static void purple_php_g_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);
static void purple_php_g_loop_callback(void);
static int purple_php_hash_index_find(HashTable *ht, void *element);
static int purple_php_heartbeat_callback(gpointer data);
static void *purple_php_request_authorize(PurpleAccount *account, const char *remote_user, const char *id, const char *alias, const char *message,
                                          gboolean on_list, PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb,void *user_data);

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
	purple_php_write_im_function,              /* write_im             */
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


static PurpleAccountUiOps php_account_uiops = 
{
	NULL,				/* notify added */
	NULL,				/* status changed */
	NULL,				/* request add */
	purple_php_request_authorize,				/* request authorize */
	NULL,				/* close account request */
	NULL,
	NULL,
	NULL,
	NULL
};

ZEND_DECLARE_MODULE_GLOBALS(purple);

void php_purple_globals_ctor(zend_purple_globals *purple_globals TSRMLS_DC)
{
	ALLOC_INIT_ZVAL(purple_globals->purple_php_client_obj);
	Z_TYPE_P(purple_globals->purple_php_client_obj) = IS_OBJECT;
	
	zend_hash_init(&(purple_globals->ppos).buddy, 20, NULL, NULL, 0);
	zend_hash_init(&(purple_globals->ppos).group, 20, NULL, NULL, 0);
	
	purple_globals->debug_enabled = 0;
	purple_globals->custom_user_directory = NULL;
	purple_globals->custom_plugin_path = NULL;
	purple_globals->ui_id = NULL;
	purple_globals->plugin_save_pref = NULL;
}
void php_purple_globals_dtor(zend_purple_globals *purple_globals TSRMLS_DC) { }

/* True global resources - no need for thread safety here */
static int le_purple;


/* classes definitions*/
static zend_class_entry *PurpleClient_ce, *PurpleConversation_ce, *PurpleAccount_ce, *PurpleConnection_ce, *PurpleBuddy_ce, *PurpleBuddyList_ce, *PurpleBuddyGroup_ce;


/* {{{ purple_functions[] */
zend_function_entry purple_functions[] = {
	{NULL, NULL, NULL}	/* Must be the last line in purple_functions[] */
};
/* }}} */

/* {{{ client class methods[] */
zend_function_entry PurpleClient_methods[] = {
	PHP_ME(PurpleClient, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, getInstance, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleClient, initInternal, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, getCoreVersion, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
	PHP_ME(PurpleClient, connectToSignal, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
	PHP_ME(PurpleClient, writeConv, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, writeIM, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, onSignedOn, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, runLoop, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
	PHP_ME(PurpleClient, addAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleClient, getProtocols, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleClient, setUserDir, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleClient, loopCallback, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, loopHeartBeat, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PurpleClient, deleteAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleClient, findAccount, NULL, ZEND_ACC_PUBLIC )
	PHP_ME(PurpleClient, authorizeRequest, NULL, ZEND_ACC_PROTECTED)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ conversation class methods[] */
zend_function_entry PurpleConversation_methods[] = {
	PHP_ME(PurpleConversation, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleConversation, getName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleConversation, sendIM, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleConversation, getAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleConversation, setAccount, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ account class methods[] */
zend_function_entry PurpleAccount_methods[] = {
	PHP_ME(PurpleAccount, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, setPassword, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, setEnabled, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, addBuddy, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, removeBuddy, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, clearSettings, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, set, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, isConnected, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, isConnecting, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, getUserName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleAccount, getPassword, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ connection class methods[] */
zend_function_entry PurpleConnection_methods[] = {
	PHP_ME(PurpleConnection, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleConnection, getAccount, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ buddy class methods[] */
zend_function_entry PurpleBuddy_methods[] = {
	PHP_ME(PurpleBuddy, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddy, getName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddy, getAlias, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddy, getGroup, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddy, getAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddy, isOnline, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ buddy list class methods[] */
zend_function_entry PurpleBuddyList_methods[] = {
	PHP_ME(PurpleBuddyList, __construct, NULL, ZEND_ACC_PRIVATE)
	PHP_ME(PurpleBuddyList, addBuddy, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleBuddyList, addGroup, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleBuddyList, findBuddy, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleBuddyList, load, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleBuddyList, findGroup, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleBuddyList, removeBuddy, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PurpleBuddyList, removeGroup, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ buddy group class methods[] */
zend_function_entry PurpleBuddyGroup_methods[] = {
	PHP_ME(PurpleBuddyGroup, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddyGroup, getAccounts, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddyGroup, getSize, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddyGroup, getOnlineCount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PurpleBuddyGroup, getName, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ purple_module_entry */
zend_module_entry purple_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"purple",
	purple_functions,
	PHP_MINIT(purple),
	PHP_MSHUTDOWN(purple),
	PHP_RINIT(purple),
	PHP_RSHUTDOWN(purple),
	PHP_MINFO(purple),
#if ZEND_MODULE_API_NO >= 20010901
	"0.2",
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PURPLE
ZEND_GET_MODULE(purple)
#endif

/* {{{ PHP_INI */
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("purple.custom_user_directory", "/dev/null", PHP_INI_ALL, OnUpdateString, custom_user_directory, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.custom_plugin_path", "", PHP_INI_ALL, OnUpdateString, custom_plugin_path, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.ui_id", "php", PHP_INI_ALL, OnUpdateString, ui_id, zend_purple_globals, purple_globals)
	STD_PHP_INI_BOOLEAN("purple.debug_enabled", "0", PHP_INI_ALL, OnUpdateBool, debug_enabled, zend_purple_globals, purple_globals)
	STD_PHP_INI_ENTRY("purple.plugin_save_pref", "/purple/nullclient/plugins/saved", PHP_INI_ALL, OnUpdateString, plugin_save_pref, zend_purple_globals, purple_globals)
PHP_INI_END()
/* }}} */


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(purple)
{
#ifdef ZTS
	ts_allocate_id(&purple_globals_id,
			sizeof(zend_purple_globals),
			(ts_allocate_ctor)php_purple_globals_ctor,
			(ts_allocate_dtor)php_purple_globals_dtor);
#else
	php_purple_globals_ctor(&purple_globals TSRMLS_CC);
#endif
	
	REGISTER_INI_ENTRIES();

	g_log_set_handler (NULL, G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL | G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_RECURSION, purple_php_g_log_handler, NULL);
	
	/* initalizing classes */
	zend_class_entry ce;
	
	/* classes definitions */

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Client", PurpleClient_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleClient", PurpleClient_methods);
#endif
	PurpleClient_ce = zend_register_internal_class(&ce TSRMLS_CC);

	/* A type of conversation */
	zend_declare_class_constant_long(PurpleClient_ce, "CONV_TYPE_UNKNOWN", sizeof("CONV_TYPE_UNKNOWN")-1, PURPLE_CONV_TYPE_UNKNOWN TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "CONV_TYPE_IM", sizeof("CONV_TYPE_IM")-1, PURPLE_CONV_TYPE_IM TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "CONV_TYPE_CHAT", sizeof("CONV_TYPE_CHAT")-1, PURPLE_CONV_TYPE_CHAT TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "CONV_TYPE_MISC", sizeof("CONV_TYPE_MISC")-1, PURPLE_CONV_TYPE_MISC TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "CONV_TYPE_ANY", sizeof("CONV_TYPE_ANY")-1, PURPLE_CONV_TYPE_ANY TSRMLS_CC);
	/* Flags applicable to a message */
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_SEND", sizeof("MESSAGE_SEND")-1, PURPLE_MESSAGE_SEND TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_RECV", sizeof("MESSAGE_RECV")-1, PURPLE_MESSAGE_RECV TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_SYSTEM", sizeof("MESSAGE_SYSTEM")-1, PURPLE_MESSAGE_SYSTEM TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_AUTO_RESP", sizeof("MESSAGE_AUTO_RESP")-1, PURPLE_MESSAGE_AUTO_RESP TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_ACTIVE_ONLY", sizeof("MESSAGE_ACTIVE_ONLY")-1, PURPLE_MESSAGE_ACTIVE_ONLY TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_NICK", sizeof("MESSAGE_NICK")-1, PURPLE_MESSAGE_NICK TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_NO_LOG", sizeof("MESSAGE_NO_LOG")-1, PURPLE_MESSAGE_NO_LOG TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_WHISPER", sizeof("MESSAGE_WHISPER")-1, PURPLE_MESSAGE_WHISPER TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_ERROR", sizeof("MESSAGE_ERROR")-1, PURPLE_MESSAGE_ERROR TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_DELAYED", sizeof("MESSAGE_DELAYED")-1, PURPLE_MESSAGE_DELAYED TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_RAW", sizeof("MESSAGE_RAW")-1, PURPLE_MESSAGE_RAW TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_IMAGES", sizeof("MESSAGE_IMAGES")-1, PURPLE_MESSAGE_IMAGES TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_NOTIFY", sizeof("MESSAGE_NOTIFY")-1, PURPLE_MESSAGE_NOTIFY TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_NO_LINKIFY", sizeof("MESSAGE_NO_LINKIFY")-1, PURPLE_MESSAGE_NO_LINKIFY TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "MESSAGE_INVISIBLE", sizeof("MESSAGE_INVISIBLE")-1, PURPLE_MESSAGE_INVISIBLE TSRMLS_CC);
	/* Flags applicable to a status */
	zend_declare_class_constant_long(PurpleClient_ce, "PURPLE_STATUS_OFFLINE", sizeof("PURPLE_STATUS_OFFLINE")-1, PURPLE_STATUS_OFFLINE TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "PURPLE_STATUS_AVAILABLE", sizeof("PURPLE_STATUS_AVAILABLE")-1, PURPLE_STATUS_AVAILABLE TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "PURPLE_STATUS_UNAVAILABLE", sizeof("PURPLE_STATUS_UNAVAILABLE")-1, PURPLE_STATUS_UNAVAILABLE TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "PURPLE_STATUS_INVISIBLE", sizeof("PURPLE_STATUS_INVISIBLE")-1, PURPLE_STATUS_INVISIBLE TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "PURPLE_STATUS_AWAY", sizeof("PURPLE_STATUS_AWAY")-1, PURPLE_STATUS_AWAY TSRMLS_CC);
	zend_declare_class_constant_long(PurpleClient_ce, "PURPLE_STATUS_MOBILE", sizeof("PURPLE_STATUS_MOBILE")-1, PURPLE_STATUS_MOBILE TSRMLS_CC);
	
#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Conversation", PurpleConversation_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleConversation", PurpleConversation_methods);
#endif
	PurpleConversation_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PurpleConversation_ce, "index", sizeof("index")-1, -1, ZEND_ACC_PRIVATE TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Account", PurpleAccount_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleAccount", PurpleAccount_methods);
#endif
	PurpleAccount_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PurpleAccount_ce, "index", sizeof("index")-1, -1, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE TSRMLS_CC);
	
#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Connection", PurpleConnection_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleConnection", PurpleConnection_methods);
#endif
	PurpleConnection_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PurpleConnection_ce, "index", sizeof("index")-1, -1, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::Buddy", PurpleBuddy_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleBuddy", PurpleBuddy_methods);
#endif
	PurpleBuddy_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PurpleBuddy_ce, "index", sizeof("index")-1, -1, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::BuddyList", PurpleBuddyList_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleBuddyList", PurpleBuddyList_methods);
#endif
	PurpleBuddyList_ce = zend_register_internal_class(&ce TSRMLS_CC);

#if ZEND_MODULE_API_NO >= 20071006
	INIT_CLASS_ENTRY(ce, "Purple::BuddyGroup", PurpleBuddyGroup_methods);
#else
	INIT_CLASS_ENTRY(ce, "PurpleBuddyGroup", PurpleBuddyGroup_methods);
#endif
	PurpleBuddyGroup_ce = zend_register_internal_class(&ce TSRMLS_CC);

	
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
	purple_accounts_set_ui_ops(&php_account_uiops);
	purple_eventloop_set_ui_ops(&glib_eventloops);
	purple_plugins_add_search_path(INI_STR("purple.custom_plugin_path"));

	if (!purple_core_init(INI_STR("purple.ui_id"))) {
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
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(purple)
{
	UNREGISTER_INI_ENTRIES();

#ifdef ZTS
	ts_free_id(purple_globals_id);
#else
	php_purple_globals_dtor(&purple_globals TSRMLS_CC);
#endif
	
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(purple)
{
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(purple)
{
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION */
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

/* {{{ */
PHP_METHOD(PurpleClient, __construct)
{

}
/* }}} */


/* {{{ proto int PurpleClient::connectToSignal(string signal)
	Connects a signal handler to a signal for a particular object */
PHP_METHOD(PurpleClient, connectToSignal)
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


/* {{{ proto void PurpleClient::runLoop(int interval)
	Creates the main loop*/
PHP_METHOD(PurpleClient, runLoop)
{
	long interval;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &interval) == FAILURE) {
		RETURN_NULL();
	}
	
	purple_php_g_loop_callback();
	
	if(interval) {
		g_timeout_add(interval, (GSourceFunc)purple_php_heartbeat_callback, NULL);
	}
	
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
}
/* }}} */


/* {{{ proto object PurpleClient::addAccount(string dsn)
	adds a new account to the current client*/
PHP_METHOD(PurpleClient, addAccount)
{
	char *account_dsn, *protocol, *nick, *password, *host, *port;
	const char *error;
	int account_dsn_len, erroffset, offsets[19], rc;
	pcre *re;
	PurpleAccount *account = NULL;
	GList *accounts;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &account_dsn, &account_dsn_len) == FAILURE) {
		RETURN_FALSE;
	}

	re = pcre_compile("([a-zA-Z]+)://([^:]+):?([^@]*)@?([a-zA-Z0-9-.]*):?([0-9]*)", 0, &error, &erroffset, NULL);

	if (re == NULL)
	{
		zend_throw_exception(NULL, "PCRE compilation failed at offset %d: %s", erroffset, error, 0 TSRMLS_CC);
		return;
	}

	rc = pcre_exec(re, NULL, account_dsn, account_dsn_len, 0, 0, offsets, 18);

	if(rc < 0)
	{
		switch(rc)
			{
			case PCRE_ERROR_NOMATCH:
				zend_throw_exception(NULL, "The account string must match \"protocol://user:password@host:port\" pattern", 0 TSRMLS_CC);
			break;

			default:
				zend_throw_exception(NULL, "The account string must match \"protocol://user:password@host:port pattern\". Matching error %d", rc, 0 TSRMLS_CC);
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
	host = emalloc(offsets[9] - offsets[8] + 1);
	php_sprintf(host, "%.*s", offsets[9] - offsets[8], account_dsn + offsets[8]);
	port = emalloc(offsets[11] - offsets[10] + 1);
	php_sprintf(port, "%.*s", offsets[11] - offsets[10], account_dsn + offsets[10]);

	account = purple_account_new(estrdup(nick), purple_php_get_protocol_id_by_name(protocol));

	if(NULL != account) {

		purple_account_set_password(account, estrdup(password));

		if(strlen(host)) {
			purple_account_set_string(account, "server", host);
		}

		if(strlen(port) && atoi(port)) {
			purple_account_set_int(account, "port", (int)atoi(port));
		}

		purple_account_set_enabled(account, INI_STR("purple.ui_id"), 1);

		purple_accounts_add(account);

		accounts = purple_accounts_get_all();

		ZVAL_NULL(return_value);
		Z_TYPE_P(return_value) = IS_OBJECT;
		object_init_ex(return_value, PurpleAccount_ce);
		zend_update_property_long(PurpleAccount_ce,
		                          return_value,
		                          "index",
		                          sizeof("index")-1,
		                          (long)g_list_position(accounts, g_list_find(accounts, account)) TSRMLS_CC
		                          );

		efree(protocol);
		efree(nick);
		efree(password);
		efree(host);
		efree(port);

		return;

	}

	efree(protocol);
	efree(nick);
	efree(password);
	efree(host);
	efree(port);
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto void PurpleClient::deleteAccount(PurpleAccount account)
	Removes an account from the list of accounts*/
PHP_METHOD(PurpleClient, deleteAccount)
{
	zval *account;
	PurpleAccount *paccount = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &account) == FAILURE) {
		RETURN_FALSE;
	}

	switch (Z_TYPE_P(account)) {
		case IS_OBJECT:
			paccount = g_list_nth_data(purple_accounts_get_all(), (guint)Z_LVAL_P(zend_read_property(PurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));
		break;
			
		case IS_STRING:
			paccount = purple_accounts_find(Z_STRVAL_P(account), NULL);
		break;
	}
		
	if(paccount) {
		purple_accounts_delete(paccount);
		
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto PurpleAccount PurpleClient::findAccount(string name)
	Finds the specified account in the accounts list */
PHP_METHOD(PurpleClient, findAccount)
{
	char *account_name;
	int account_name_len;
	zval *account;
	PurpleAccount *paccount = NULL;
	GList *l;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &account_name, &account_name_len) == FAILURE) {
		RETURN_NULL();
	}

	paccount = purple_accounts_find(account_name, NULL);

	if(paccount) {
		ZVAL_NULL(return_value);
		Z_TYPE_P(return_value) = IS_OBJECT;
		object_init_ex(return_value, PurpleAccount_ce);
		zend_update_property_long(PurpleAccount_ce,
		                          return_value,
		                          "index",
		                          sizeof("index")-1,
		                          (long)g_list_position(purple_accounts_get_all(),
		                          g_list_find(purple_accounts_get_all(), paccount)) TSRMLS_CC
		                         );
		return;
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PurpleClient::getCoreVersion(void)
	Returns the libpurple core version string */
PHP_METHOD(PurpleClient, getCoreVersion)
{	
	char *version = estrdup(purple_core_get_version());

	RETURN_STRING(version, 0);
}
/* }}} */


/* {{{ proto object PurpleClient::getInstance(void)
	creates new PurpleClient instance*/
PHP_METHOD(PurpleClient, getInstance)
{
	zval_ptr_dtor(&return_value);

	if(NULL == zend_objects_get_address(PURPLE_G(purple_php_client_obj) TSRMLS_CC)) {
		MAKE_STD_ZVAL(PURPLE_G(purple_php_client_obj));
		Z_TYPE_P(PURPLE_G(purple_php_client_obj)) = IS_OBJECT;
#if ZEND_MODULE_API_NO >= 20071006
		object_init_ex(PURPLE_G(purple_php_client_obj), EG(called_scope));
#else
		zend_class_entry **ce = NULL;
		zend_hash_find(EG(class_table), "custompurpleclient", sizeof("custompurpleclient"), (void **) &ce);

		if(ce && (*ce)->parent && 0 == strcmp("PurpleClient", (*ce)->parent->name)) {
			object_init_ex(PURPLE_G(purple_php_client_obj), *ce);
		} else {
			zend_throw_exception(NULL, "The PurpleClient child class must be named CustomPurpleClient for php < 5.3", 0 TSRMLS_CC);
			return;
		}
		/* object_init_ex(tmp, EG(current_execute_data->fbc->common.scope)); would be beautiful but works not as expected */
		
#endif
		*return_value = *PURPLE_G(purple_php_client_obj);

		call_custom_method(&PURPLE_G(purple_php_client_obj),
		                   Z_OBJCE_P(PURPLE_G(purple_php_client_obj)),
		                   NULL,
		                   "initinternal",
		                   sizeof("initinternal")-1,
		                   NULL,
		                  0);

		return;
	}

	*return_value = *PURPLE_G(purple_php_client_obj);
	
	return;
}
/* }}} */


/* {{{ proto array PurpleClient::getProtocols(void)
	Returns a list of all valid protocol plugins */
PHP_METHOD(PurpleClient, getProtocols)
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

	efree(protocols);
	g_list_free(iter);
	
	return;
}
/* }}} */


/* {{{ proto void PurpleClient::setUserDir([string $userDir])
	Define a custom purple settings directory, overriding the default (user's home directory/.purple) */
PHP_METHOD(PurpleClient, setUserDir) {
	char *user_dir;
	int user_dir_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &user_dir, &user_dir_len) == FAILURE) {
		RETURN_NULL();
	}
	
	user_dir = !user_dir_len ? INI_STR("purple.custom_user_directory") : estrdup(user_dir);
	PURPLE_G(custom_user_directory) = estrdup(user_dir);
	
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

/* {{{ proto void PurpleClient::writeConv(PurpleConversation conversation, PurplePuddy buddy, string message, int flags, timestamp time)
	this callback method writes to the conversation, if implemented*/
PHP_METHOD(PurpleClient, writeConv)
{
}
/* }}} */


/* {{{ proto void PurpleClient::writeIM(PurpleConversation conversation, PurplePuddy buddy, string message, int flags, timestamp time)
	this callback method writes to the conversation, if implemented*/
PHP_METHOD(PurpleClient, writeIM)
{
}
/* }}} */

/* {{{ proto void PurpleClient::onSignedOn(PurpleConnection connection)
	this callback is called at the moment, where the client gets singed on, if implemented */
PHP_METHOD(PurpleClient, onSignedOn)
{
}
/* }}} */


/* {{{ proto void PurpleClient::initInternal(void)
	thes callback method is called within the PurpleClient::getInstance, so if implemented, can initalize some internal stuff*/
PHP_METHOD(PurpleClient, initInternal)
{
}
/* }}}*/


/* {{{ proto void PurpleClient::loopCallback(void)
	this callback method is called within the PurpleClient::runLoop */
PHP_METHOD(PurpleClient, loopCallback)
{
}
/* }}} */


/* {{{ proto void loopHeartBeat(void) 
	this callback method is invoked by glib timer */
PHP_METHOD(PurpleClient, loopHeartBeat)
{
}
/* }}} */


/* {{{ proto boolean PurpleClient::authorizeRequest(PurpleAccount account, string $remote_user, string $message, boolean $on_list)
	this callback method is invoked, when someone adds us to his buddy list */
PHP_METHOD(PurpleClient, authorizeRequest)
{
	
}
/* }}} */


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

/* {{{ proto object PurpleConnection::__construct()
	constructor*/
PHP_METHOD(PurpleConnection, __construct)
{
	
}
/* }}} */


/* {{{ proto PurpleAccount PurpleConnection::getAccount()
		Returns the connection's account*/
PHP_METHOD(PurpleConnection, getAccount)
{
	PurpleConnection *conn = NULL;
	PurpleAccount *acc = NULL;
	GList *accounts = NULL;

	conn = g_list_nth_data (purple_connections_get_all(), (guint)Z_LVAL_P(zend_read_property(PurpleConnection_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	if(NULL != conn) {
		acc = purple_connection_get_account(conn);
		if(NULL != acc) {
			accounts = purple_accounts_get_all();

			ZVAL_NULL(return_value);
			Z_TYPE_P(return_value) = IS_OBJECT;
			object_init_ex(return_value, PurpleAccount_ce);
			zend_update_property_long(PurpleAccount_ce,
			                          return_value,
			                          "index",
			                          sizeof("index")-1,
			                          (long)g_list_position(accounts, g_list_find(accounts, acc)) TSRMLS_CC
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

/* {{{ proto object PurpleAccount::__construct(string user_name, string protocol_name)
	Creates a new account*/
PHP_METHOD(PurpleAccount, __construct)
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

		zend_update_property_long(PurpleAccount_ce,
		                          getThis(),
		                          "index",
		                          sizeof("index")-1,
		                          (long)g_list_position(accounts, g_list_last(accounts)) TSRMLS_CC
		                          );
		return;
		
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto void PurpleAccount::setPassword(int account, string password)
	Sets the account's password */
PHP_METHOD(PurpleAccount, setPassword)
{
	int password_len;
	char *password;
	PurpleAccount *account = NULL;
	zval *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));

	/*php_printf("account_index = %d\n", Z_LVAL_P(account_index));*/

 	account = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != account) {
		purple_account_set_password(account, estrdup(password));
	}
}
/* }}} */


/* {{{ proto void PurpleAccount::setEnabled(bool enabled)
	Sets whether or not this account is enabled for some UI */
PHP_METHOD(PurpleAccount, setEnabled)
{
	zend_bool enabled;
	PurpleAccount *account = NULL;
	zval *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &enabled) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	account = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != account) {
		purple_account_set_enabled(account, INI_STR("purple.ui_id"), (gboolean) enabled);
	}
}
/* }}} */


/* {{{ proto bool PurpleAccount::addBuddy(PurpleBuddy buddy)
	Adds a buddy to the server-side buddy list for the specified account */
PHP_METHOD(PurpleAccount, addBuddy)
{
	PurpleAccount *paccount = NULL;
	PurpleBuddy *pbuddy = NULL;
	zval *account_index, *buddy_index, *buddy;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PurpleBuddy_ce) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != paccount) {
		struct php_purple_object_storage *pp = &PURPLE_G(ppos);

		buddy_index = zend_read_property(PurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
		zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(buddy_index), (void**)&pbuddy);

		if(pbuddy) {
			purple_account_add_buddy(paccount, pbuddy);
			RETURN_TRUE;
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool PurpleAccount::removeBuddy(PurpleBuddy buddy)
	Removes a buddy from the server-side buddy list for the specified account */
PHP_METHOD(PurpleAccount, removeBuddy)
{
	PurpleAccount *paccount = NULL;
	PurpleBuddy *pbuddy = NULL;
	zval *account_index, *buddy_index, *buddy;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PurpleBuddy_ce) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != paccount) {
		struct php_purple_object_storage *pp = &PURPLE_G(ppos);

		buddy_index = zend_read_property(PurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
		zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(buddy_index), (void**)&pbuddy);

		if(pbuddy) {
			purple_account_remove_buddy(paccount, pbuddy, purple_buddy_get_group(pbuddy));
			RETURN_TRUE;
		}
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto void PurpleAccount::clearSettings(void)
	Clears all protocol-specific settings on an account. }}} */
PHP_METHOD(PurpleAccount, clearSettings)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		purple_account_clear_settings(paccount);
		RETURN_TRUE;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto void PurpleAccount::set(string name, string value)
	Sets a protocol-specific setting for an account.
	The value types expected are int, string or bool. */
PHP_METHOD(PurpleAccount, set)
{
	PurpleAccount *paccount = NULL;
	zval *index, *value;
	char *name;
	int name_len;
	
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &value) == FAILURE) {
		RETURN_FALSE;
	}
	
	index = zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		switch (Z_TYPE_P(value)) {
			case IS_BOOL:
				purple_account_set_bool (paccount, name, (gboolean) Z_LVAL_P(value));
			break;
			
			case IS_LONG:
			case IS_DOUBLE:
				purple_account_set_int (paccount, name, (int) Z_LVAL_P(value));
			break;
				
			case IS_STRING:
				purple_account_set_string (paccount, name, Z_STRVAL_P(value));
			break;
				
			default:
				RETURN_FALSE;
			break;
		}
	
		RETURN_TRUE;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto boolean PurpleAccount::isConnected(void)
	Returns whether or not the account is connected*/
PHP_METHOD(PurpleAccount, isConnected)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETVAL_BOOL((long) purple_account_is_connected(paccount));
		return;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto boolean PurpleAccount::isConnecting(void)
	Returns whether or not the account is connecting*/
PHP_METHOD(PurpleAccount, isConnecting)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETVAL_BOOL((long) purple_account_is_connecting(paccount));
		return;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto string PurpleAccount::getUserName(void) Returns the account's username */
PHP_METHOD(PurpleAccount, getUserName)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETURN_STRING(estrdup(purple_account_get_username(paccount)), 0);
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PurpleAccount::getPassword(void) Returns the account's password */
PHP_METHOD(PurpleAccount, getPassword)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETURN_STRING(estrdup(purple_account_get_password(paccount)), 0);
	}
	
	RETURN_NULL();
}
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

/* {{{ proto int PurpleConversation::__construct(int type, PurpleAccount account, string name)
	Creates a new conversation of the specified type */
PHP_METHOD(PurpleConversation, __construct)
{
	int type, name_len;
	char *name;
	PurpleConversation *conv = NULL;
	PurpleAccount *paccount = NULL;
	GList *conversations = NULL;
	zval *account, *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lOs", &type, &account, PurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	account_index = zend_read_property(PurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC);
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));

	if(NULL != account) {
		conv = purple_conversation_new(type, paccount, estrdup(name));
		conversations = purple_get_conversations();

		zend_update_property_long(	PurpleConversation_ce,
									getThis(),
									"index",
									sizeof("index")-1,
									(long)g_list_position(conversations, g_list_last(conversations)) TSRMLS_CC
									);
		return;
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PurpleConversation::getName(void)
	Returns the specified conversation's name*/
PHP_METHOD(PurpleConversation, getName)
{
	zval *conversation_index;
	PurpleConversation *conversation = NULL;

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(PurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		RETURN_STRING(estrdup(purple_conversation_get_name(conversation)), 0);
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto void PurpleConversation::sendIM(string message)
	Sends a message to this IM conversation */
PHP_METHOD(PurpleConversation, sendIM)
{
	int message_len;
	char *message;
	PurpleConversation *conversation = NULL;
	zval *conversation_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &message, &message_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(PurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		purple_conv_im_send (PURPLE_CONV_IM(conversation), estrdup(message));
	}
}
/* }}} */


/* {{{ proto PurpleAccount PurpleConversation::getAccount(void)
	Gets the account of this conversation*/
PHP_METHOD(PurpleConversation, getAccount)
{
	PurpleConversation *conversation = NULL;
	PurpleAccount *acc = NULL;
	zval *conversation_index;

	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(zend_read_property(PurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	if(NULL != conversation) {
		acc = purple_conversation_get_account(conversation);
		if(NULL != acc) {
			ZVAL_NULL(return_value);
			Z_TYPE_P(return_value) = IS_OBJECT;
			object_init_ex(return_value, PurpleAccount_ce);
			zend_update_property_long(	PurpleAccount_ce,
										return_value,
										"index",
										sizeof("index")-1,
										(long)g_list_position(purple_accounts_get_all(),
										g_list_find(purple_accounts_get_all(), acc)) TSRMLS_CC
										);
			return;
		}
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto void PurpleConversation::setAccount(PurpleAccount account)
	Sets the specified conversation's purple_account */
PHP_METHOD(PurpleConversation, setAccount)
{
	PurpleConversation *pconv = NULL;
	PurpleAccount *paccount = NULL;
	zval *account;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &account, PurpleAccount_ce) == FAILURE) {
		RETURN_NULL();
	}
	
	pconv = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(zend_read_property(PurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	if(pconv) {
		paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(zend_read_property(PurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));
		
		if(account) {
			purple_conversation_set_account(pconv, paccount);
		}
	}
}
/* }}} */

/*
**
**
** End purple conversation methods
**
*/


/*
**
**
** Purple Buddy methods
**
*/

/* {{{ proto object PurpleBuddy::__construct(PurpleAccount account, string name, string alias)
	Creates new buddy*/
PHP_METHOD(PurpleBuddy, __construct)
{
	PurpleAccount *paccount = NULL;
	PurpleBuddy *pbuddy = NULL;
	char *name, *alias;
	int name_len, alias_len, account_index;
	zval *account;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|s", &account, PurpleAccount_ce, &name, &name_len, &alias, &alias_len) == FAILURE) {
		RETURN_NULL();
	}

	paccount = g_list_nth_data (purple_accounts_get_all(), Z_LVAL_P(zend_read_property(PurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));

	if(paccount) {
		pbuddy = purple_find_buddy(paccount, name);
		struct php_purple_object_storage *pp = &PURPLE_G(ppos);

		if(pbuddy) {

			int ind = purple_php_hash_index_find(&pp->buddy, pbuddy);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(	PurpleBuddy_ce,
											getThis(),
											"index",
											sizeof("index")-1,
											(long)nextid TSRMLS_CC
											);
			} else {
				zend_update_property_long(	PurpleBuddy_ce,
											getThis(),
											"index",
											sizeof("index")-1,
											(long)ind TSRMLS_CC
											);
			}

			return;
		} else {
			pbuddy = purple_buddy_new(paccount, name, alias ? alias : NULL);
			ulong nextid = zend_hash_next_free_element(&pp->buddy);
			zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
			zend_update_property_long(	PurpleBuddy_ce,
										getThis(),
										"index",
										sizeof("index")-1,
										(long)nextid TSRMLS_CC
										);

			return;
		}
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PurpleBuddy::getName(void)
	Gets buddy name*/
PHP_METHOD(PurpleBuddy, getName)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;

	index = zend_read_property(PurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PURPLE_G(ppos).buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		const char *name = purple_buddy_get_name(pbuddy);
		if(name) {
			RETURN_STRING(estrdup(name), 0);
		}
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PurpleBuddy::getAlias(void)
	gets buddy alias */
PHP_METHOD(PurpleBuddy, getAlias)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;

	index = zend_read_property(PurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PURPLE_G(ppos).buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		char const *alias = purple_buddy_get_alias_only(pbuddy);
		RETURN_STRING( alias && *alias ? estrdup(alias) : "", 0);
	}
	
	RETURN_NULL();
	
}
/* }}} */


/* {{{ proto PurpleGroup PurpleBuddy::getGroup(void)
	gets buddy's group */
PHP_METHOD(PurpleBuddy, getGroup)
{
	zval *index, *tmp;
	PurpleBuddy *pbuddy = NULL;
	PurpleGroup *pgroup = NULL;
	struct php_purple_object_storage *pp = &PURPLE_G(ppos);
			
	index = zend_read_property(PurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		PURPLE_MK_OBJ(tmp, PurpleBuddyGroup_ce);

		pgroup = purple_buddy_get_group(pbuddy);
		if(pgroup) {
			int ind = purple_php_hash_index_find(&pp->group, pgroup);
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->group);
				zend_hash_index_update(&pp->group, nextid, pgroup, sizeof(PurpleGroup), NULL);
				zend_update_property_long(	PurpleBuddyGroup_ce,
											tmp,
											"index",
											sizeof("index")-1,
											(long)nextid TSRMLS_CC
											);
			} else {
				zend_update_property_long(	PurpleBuddyGroup_ce,
											tmp,
											"index",
											sizeof("index")-1,
											(long)ind TSRMLS_CC
											);
			}

			*return_value = *tmp;

			return;
		}
	}

	RETURN_NULL();
	
}
/* }}} */


/* {{{ proto PurpleAccount PurpleBuddy::getAccount(void)
	gets buddy's account */
PHP_METHOD(PurpleBuddy, getAccount)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;
	PurpleAccount *paccount = NULL;
	struct php_purple_object_storage *pp = &PURPLE_G(ppos);
	GList *accounts = NULL;
			
	index = zend_read_property(PurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		PURPLE_MK_OBJ(return_value, PurpleAccount_ce);

		paccount = purple_buddy_get_account(pbuddy);
		if(paccount) {
			accounts = purple_accounts_get_all();

			zend_update_property_long(	PurpleAccount_ce,
							return_value,
							"index",
							sizeof("index")-1,
							(long)g_list_position(accounts, g_list_last(accounts)) TSRMLS_CC
						);
			return;
		}
	}

	RETURN_NULL();
}
/* }}} */



/* {{{ proto bool PurpleBuddy::isOnline(void)
	checks weither the buddy is online */
PHP_METHOD(PurpleBuddy, isOnline)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);
			
	index = zend_read_property(PurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	RETVAL_BOOL(PURPLE_BUDDY_IS_ONLINE(pbuddy));
}
/* }}} */

/*
**
**
** End purple Buddy methods
**
*/


/*
**
**
** Purple BuddyList methods
**
*/

/* {{{ proto PurpleBuddyList PurpleBuddyList::__construct(void)
	should newer be called*/
PHP_METHOD(PurpleBuddyList, __construct)
{
}
/* }}} */


/* {{{ proto bool PurpleBuddyList::addBuddy(PurpleBuddy buddy[, PurpleGroup group])
	adds the buddy to the blist (optionally to the given group in the blist, not implemented yet)*/
PHP_METHOD(PurpleBuddyList, addBuddy)
{
	zval *buddy, *group, *index;
	PurpleBuddy *pbuddy = NULL;
	PurpleGroup *pgroup = NULL;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|O", &buddy, PurpleBuddy_ce, &group, PurpleBuddyGroup_ce) == FAILURE) {
		RETURN_NULL();
	}

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);

	index = zend_read_property(PurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	index = zend_read_property(PurpleBuddyGroup_ce, group, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->group, (ulong)Z_LVAL_P(index), (void**)&pgroup);
	
	if(pbuddy && pgroup) {
		purple_blist_add_buddy(pbuddy, NULL, pgroup, NULL);
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto bool PurpleBuddyList::addGroup(string name)
	Adds new group to the blist */
PHP_METHOD(PurpleBuddyList, addGroup)
{
	zval *group, *index;
	PurpleGroup *pgroup;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PurpleBuddyGroup_ce) == FAILURE) {
		RETURN_NULL();
	}

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);
	
	index = zend_read_property(PurpleBuddyGroup_ce, group, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
		purple_blist_add_group(pgroup, NULL);
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto PurpleBuddy PurpleBuddyList::findBuddy(PurpleAccount account, string name)
	returns the buddy, if found */
PHP_METHOD(PurpleBuddyList, findBuddy)
{
	zval *account, *index, *buddy;
	char *name;
	int name_len;
	PurpleBuddy *pbuddy;
	PurpleAccount *paccount;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os", &account, PurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);

	index = zend_read_property(PurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC);
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));

	if(paccount) {
		pbuddy = purple_find_buddy(paccount, name);

		if(pbuddy) {
			int ind = purple_php_hash_index_find(&pp->buddy, pbuddy);
			PURPLE_MK_OBJ(buddy, PurpleBuddy_ce);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(	PurpleBuddy_ce,
											buddy,
											"index",
											sizeof("index")-1,
											(long)nextid TSRMLS_CC
											);
			} else {
				zend_update_property_long(	PurpleBuddy_ce,
											buddy,
											"index",
											sizeof("index")-1,
											(long)ind TSRMLS_CC
											);
			}

			*return_value = *buddy;

			return;
		}
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto void PurpleBuddyList::load(void)
	loads the blist.xml from the homedir */
PHP_METHOD(PurpleBuddyList, load)
{
	purple_blist_load();

	purple_set_blist(purple_get_blist());
}
/* }}} */


/* {{{ proto PurpleGroup PurpleBuddyList::findGroup(string group)
	Finds group by name }}} */
PHP_METHOD(PurpleBuddyList, findGroup)
{
	PurpleGroup *pgroup = NULL;
	zval *name, *retval_ptr;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		RETURN_NULL();
	}

	pgroup = purple_find_group(Z_STRVAL_P(name));

	if(pgroup) {
		zval ***params;
		zend_fcall_info fci;
		zend_fcall_info_cache fcc;

		params = safe_emalloc(sizeof(zval **), 1, 0);
		params[0] = &name;
		
		object_init_ex(return_value, PurpleBuddyGroup_ce);
		
		fci.size = sizeof(fci);
		fci.function_table = EG(function_table);
		fci.function_name = NULL;
		fci.symbol_table = NULL;
		fci.object_pp = &return_value;
		fci.retval_ptr_ptr = &retval_ptr;
		fci.param_count = 1;
		fci.params = params;
		fci.no_separation = 1;

		fcc.initialized = 1;
		fcc.function_handler = PurpleBuddyGroup_ce->constructor;
		fcc.calling_scope = EG(scope);
		fcc.object_pp = &return_value;
		
		if (zend_call_function(&fci, &fcc TSRMLS_CC) == FAILURE) {
			efree(params);
			zval_ptr_dtor(&retval_ptr);
			zend_error(E_WARNING, "Invocation of %s's constructor failed", PurpleBuddyGroup_ce->name);
			RETURN_NULL();
		}
		if (retval_ptr) {
			zval_ptr_dtor(&retval_ptr);
		}
		efree(params);
		
		return;
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto boolean PurpleBuddyList::removeBuddy(PurpleBuddy buddy)
	Removes a buddy from the buddy list */
PHP_METHOD(PurpleBuddyList, removeBuddy)
{
	zval *buddy, *index;
	PurpleBuddy *pbuddy = NULL;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PurpleBuddy_ce) == FAILURE) {
		RETURN_FALSE;
	}

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);

	index = zend_read_property(PurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		purple_blist_remove_buddy(pbuddy);
		zend_hash_index_del(&pp->buddy, (ulong)Z_LVAL_P(index));
		zend_hash_clean(&pp->buddy);
		zval_ptr_dtor(&buddy);

		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto boolean PurpleBuddyList::removeGroup(PurpleBuddyGroup group)
	Removes an empty group from the buddy list */
PHP_METHOD(PurpleBuddyList, removeGroup)
{
	zval *group, *index;
	PurpleGroup *pgroup = NULL;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PurpleBuddyGroup_ce) == FAILURE) {
		RETURN_FALSE;
	}

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);

	index = zend_read_property(PurpleBuddyGroup_ce, group, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
		PurpleBlistNode *node = (PurpleBlistNode *) group;

		if(node->child) {
			/* group isn't empty */
			RETURN_FALSE;
		}
		
		purple_blist_remove_group(pgroup);
		zend_hash_index_del(&pp->group, (ulong)Z_LVAL_P(index));
		zend_hash_clean(&pp->group);
		zval_ptr_dtor(&group);

		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/*
**
**
** End purple BuddyList methods
**
*/


/*
**
**
** Purple BuddyGroup methods
**
*/

/* {{{ proto object PurpleBuddyGroup::__construct(void)
	constructor*/
PHP_METHOD(PurpleBuddyGroup, __construct)
{
	PurpleGroup *pgroup = NULL;
	char *name;
	int name_len;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	struct php_purple_object_storage *pp = &PURPLE_G(ppos);
	pgroup = purple_find_group(name);

	if(pgroup) {

		int ind = purple_php_hash_index_find(&pp->group, pgroup);

		if(ind == FAILURE) {
			ulong nextid = zend_hash_next_free_element(&pp->group);
			zend_hash_index_update(&pp->group, nextid, pgroup, sizeof(PurpleGroup), NULL);
			zend_update_property_long(	PurpleBuddyGroup_ce,
										getThis(),
										"index",
										sizeof("index")-1,
										(long)nextid TSRMLS_CC
										);
		} else {
			zend_update_property_long(	PurpleBuddyGroup_ce,
										getThis(),
										"index",
										sizeof("index")-1,
										(long)ind TSRMLS_CC
										);
		}

		return;
	} else {
		pgroup = purple_group_new(name);
		ulong nextid = zend_hash_next_free_element(&pp->group);
		zend_hash_index_update(&pp->group, nextid, pgroup, sizeof(PurpleGroup), NULL);
		zend_update_property_long(	PurpleBuddyGroup_ce,
									getThis(),
									"index",
									sizeof("index")-1,
									(long)nextid TSRMLS_CC
									);

		return;
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto array PurpleBuddyGroup::getAccounts(void)
	gets all the accounts related to the group */
PHP_METHOD(PurpleBuddyGroup, getAccounts)
{
	PurpleGroup *pgroup = NULL;
	zval *index, *account;

	index = zend_read_property(PurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);
	
	if(pgroup) {
		GSList *iter = purple_group_get_accounts(pgroup);
		
		if(iter && g_slist_length(iter)) {
			array_init(return_value);
			PURPLE_MK_OBJ(account, PurpleAccount_ce);
			
			for (; iter; iter = iter->next) {
				PurpleAccount *paccount = iter->data;
				
				if (paccount) {
					zend_update_property_long(	PurpleAccount_ce,
												account,
												"index",
												sizeof("index")-1,
												(long)g_list_position(purple_accounts_get_all(),g_list_find(purple_accounts_get_all(), (gconstpointer)paccount)) TSRMLS_CC
												);
					add_next_index_zval(return_value, account);
				}
			}

			return;
		}
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto int PurpleBuddyGroup::getSize(void)
	gets the count of the buddies in the group */
PHP_METHOD(PurpleBuddyGroup, getSize)
{
	PurpleGroup *pgroup = NULL;
	zval *index;
	
	index = zend_read_property(PurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
			RETURN_LONG(purple_blist_get_group_size(pgroup, (gboolean)TRUE));
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto int PurpleBuddyGroup::getOnlineCount(void)
	gets the count of the buddies in the group with the status online*/
PHP_METHOD(PurpleBuddyGroup, getOnlineCount)
{
	PurpleGroup *pgroup = NULL;
	zval *index;

	index = zend_read_property(PurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
			RETURN_LONG(purple_blist_get_group_online_count(pgroup));
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PurpleBuddyGroup::getName(void)
	gets the name of the group */
PHP_METHOD(PurpleBuddyGroup, getName)
{
	PurpleGroup *pgroup = NULL;
	zval *index;

	index = zend_read_property(PurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
		const char *name = purple_group_get_name(pgroup);
		if(name) {
			RETURN_STRING(estrdup(name), 0);
		}
	}
	
	RETURN_NULL();
}
/* }}} */

/*
**
**
** End purple BuddyGroup methods
**
*/

/*
**
** Helper functions
**
*/

/* {{{ */
static void
purple_php_ui_init()
{
	purple_conversations_set_ui_ops(&php_conv_uiops);
}
/* }}} */


#ifdef HAVE_SIGNAL_H
/* {{{ */
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
/* }}} */


/* {{{ */
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
/* }}} */
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
/* }}} */


/* {{{ */
static zval *purple_php_long_zval(long l)
{
	zval *ret;
	MAKE_STD_ZVAL(ret);

	Z_TYPE_P(ret) = IS_LONG;
	Z_LVAL_P(ret) = l;

	return ret;
}
/* }}} */


/* {{{ */
static void purple_glib_io_destroy(gpointer data)
{
	g_free(data);
}
/* }}} */


/* {{{ */
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
/* }}} */


/* {{{ */
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
/* }}} */


/* {{{ */
static void
purple_php_write_conv_function(PurpleConversation *conv, const char *who, const char *alias, const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const int PARAMS_COUNT = 5;
	zval ***params, *conversation, *buddy, *datetime, *retval, *tmp1, *tmp2, *tmp3;
	GList *conversations = purple_get_conversations();
	PurpleBuddy *pbuddy = NULL;
	PurpleAccount *paccount = NULL;

	TSRMLS_FETCH();

	PURPLE_MK_OBJ(conversation, PurpleConversation_ce);
	zend_update_property_long(PurpleConversation_ce,
	                          conversation,
	                          "index",
	                          sizeof("index")-1,
	                          (long)g_list_position(conversations, g_list_find(conversations, conv)) TSRMLS_CC
	                          );

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), g_list_position(purple_accounts_get_all(),g_list_find(purple_accounts_get_all(), (gconstpointer)purple_conversation_get_account(conv))));
	if(paccount) {
		pbuddy = purple_find_buddy(paccount, !who ? purple_conversation_get_name(conv) : who);
		
		if(pbuddy) {
			struct php_purple_object_storage *pp = &PURPLE_G(ppos);
			int ind = purple_php_hash_index_find(&pp->buddy, pbuddy);
			PURPLE_MK_OBJ(buddy, PurpleBuddy_ce);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(PurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)ind TSRMLS_CC
				                          );
			}
		} else {
			if(who) {
				buddy = purple_php_string_zval(who);
			} else {
				buddy = purple_php_string_zval("");
			}
		}
	}
	
	tmp1 = purple_php_string_zval(message);
	tmp2 = purple_php_long_zval((long)flags);
	tmp3 = purple_php_long_zval((long)mtime);

	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "writeconv",
	                   sizeof("writeconv")-1,
	                   NULL,
	                   PARAMS_COUNT,
	                   &conversation,
	                   &buddy,
	                   &tmp1,
	                   &tmp2,
	                   &tmp3
	                   );

	zval_ptr_dtor(&tmp1);
	zval_ptr_dtor(&tmp2);
	zval_ptr_dtor(&tmp3);
}
/* }}} */


/* {{{ */
static void
purple_php_write_im_function(PurpleConversation *conv, const char *who, const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const int PARAMS_COUNT = 5;
	zval ***params, *conversation, *buddy, *datetime, *retval, *tmp1, *tmp2, *tmp3;
	GList *conversations = purple_get_conversations();
	PurpleBuddy *pbuddy = NULL;
	PurpleAccount *paccount = NULL;

	TSRMLS_FETCH();

	PURPLE_MK_OBJ(conversation, PurpleConversation_ce);
	zend_update_property_long(PurpleConversation_ce,
	                          conversation,
	                          "index",
	                          sizeof("index")-1,
	                          (long)g_list_position(conversations, g_list_find(conversations, conv)) TSRMLS_CC
	                          );

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), g_list_position(purple_accounts_get_all(),g_list_find(purple_accounts_get_all(), (gconstpointer)purple_conversation_get_account(conv))));
	if(paccount) {
		pbuddy = purple_find_buddy(paccount, !who ? purple_conversation_get_name(conv) : who);
		
		if(pbuddy) {
			struct php_purple_object_storage *pp = &PURPLE_G(ppos);
			int ind = purple_php_hash_index_find(&pp->buddy, pbuddy);
			PURPLE_MK_OBJ(buddy, PurpleBuddy_ce);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(PurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)ind TSRMLS_CC
				                         );
			}
		} else {
			if(who) {
				buddy = purple_php_string_zval(who);
			} else {
				buddy = purple_php_string_zval("");
			}
		}
	}
	
	tmp1 = purple_php_string_zval(message);
	tmp2 = purple_php_long_zval((long)flags);
	tmp3 = purple_php_long_zval((long)mtime);

	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "writeim",
	                   sizeof("writeim")-1,
	                   NULL,
	                   PARAMS_COUNT,
	                   &conversation,
	                   &buddy,
	                   &tmp1,
	                   &tmp2,
	                   &tmp3
	                   );

	zval_ptr_dtor(&tmp1);
	zval_ptr_dtor(&tmp2);
	zval_ptr_dtor(&tmp3);

}
/* }}} */


/* {{{ */
static void
purple_php_signed_on_function(PurpleConnection *conn, gpointer null)
{
	zval *connection, *retval;
	GList *connections = NULL;
	
	TSRMLS_FETCH();

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	connections = purple_connections_get_all();

	PURPLE_MK_OBJ(connection, PurpleConnection_ce);
	zend_update_property_long(PurpleConnection_ce,
	                          connection,
	                          "index",
	                          sizeof("index")-1,
	                          (long)g_list_position(connections, g_list_find(connections, conn)) TSRMLS_CC
	                          );

	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "onsignedon",
	                   sizeof("onsignedon")-1,
	                   NULL,
	                   1,
	                   &connection);
	
	zval_ptr_dtor(&connection);
}
/* }}} */


/* {{{
 Only returns the returned zval if retval_ptr != NULL */
static zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... )
{
	TSRMLS_FETCH();
	
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
	}
	va_end(given_params);
	
	fci.size = sizeof(fci);
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


/* {{{ */
static char *purple_php_tolower(const char *s)
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
static char *purple_php_get_protocol_id_by_name(const char *protocol_name)
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


/* {{{ */
static int purple_php_hash_index_find(HashTable *ht, void *element)
{
	ulong i;

	for(i=0; i<zend_hash_num_elements(ht); i++) {
		if(zend_hash_index_find(ht, i, &element) != FAILURE) {
			return (int)i;
		}
	}

	return FAILURE;
}
/* }}} */


/* {{{ */
static void
purple_php_g_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
	/**
	 * @todo call here some php callback
	 */
}
/* }}} */


/* {{{ */
static void purple_php_g_loop_callback(void)
{
	TSRMLS_FETCH();

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);

	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "loopcallback",
	                   sizeof("loopcallback")-1,
	                   NULL,
	                   0);
}
/* }}} */


/* {{{ */
static int purple_php_heartbeat_callback(gpointer data)
{
	TSRMLS_FETCH();

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);

	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "loopheartbeat",
	                   sizeof("loopheartbeat")-1,
	                   NULL,
	                   0);
	
	return 1;
}
/* }}} */

/* {{{ */
static void *
purple_php_request_authorize(PurpleAccount *account,
                             const char *remote_user,
                             const char *id,
                             const char *alias,
                             const char *message,
                             gboolean on_list,
                             PurpleAccountRequestAuthorizationCb auth_cb,
                             PurpleAccountRequestAuthorizationCb deny_cb,
                             void *user_data)
{
	TSRMLS_FETCH();

	zval *client = PURPLE_G(purple_php_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	/**
	 * @todo make account obj (now it's null)
	 */
	
	zval *result, *php_account, *php_on_list;
	ALLOC_INIT_ZVAL(php_account);
	ZVAL_BOOL(php_on_list, (long)on_list);
	
	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "authorizerequest",
	                   sizeof("authorizerequest")-1,
	                   &result,
	                   4,
	                   php_account,
	                   purple_php_string_zval(remote_user),
	                   purple_php_string_zval(message),
	                   php_on_list
	                   );
	
	if(Z_TYPE_P(result) == IS_BOOL || Z_TYPE_P(result) == IS_LONG || Z_TYPE_P(result) == IS_DOUBLE) {
		if((gboolean) Z_LVAL_P(result)) {
			auth_cb(user_data);
		} else {
			deny_cb(user_data);
		}
		
	}

	return (int*)1;
}
/* }}} */


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
