/**
 * Copyright (c) 2007-2008, Anatoliy Belsky
 *
 * This file is part of PHPurple.
 *
 * PHPhurple is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHPhurple is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHPhurple.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include <pcre/pcrelib/pcre.h>

#include "php_phurple.h"

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

#define PHURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PHURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

static void phurple_ui_init();
static zval *phurple_string_zval(const char *str);
static zval *phurple_long_zval(long l);
static void phurple_glib_io_destroy(gpointer data);
static gboolean phurple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data);
static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function, gpointer data);
static void phurple_write_conv_function(PurpleConversation *conv, const char *who, const char *alias, const char *message, PurpleMessageFlags flags, time_t mtime);
static void phurple_write_im_function(PurpleConversation *conv, const char *who, const char *message, PurpleMessageFlags flags, time_t mtime);
static void phurple_signed_on_function(PurpleConnection *gc, gpointer null);
static void phurple_signed_off_function(PurpleConnection *gc, gpointer null);
static zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... );
static char *phurple_tolower(const char *s);
static char *phurple_get_protocol_id_by_name(const char *name);
static void phurple_g_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);
static void phurple_g_loop_callback(void);
static int phurple_hash_index_find(HashTable *ht, void *element);
static int phurple_heartbeat_callback(gpointer data);
static void *phurple_request_authorize(PurpleAccount *account, const char *remote_user, const char *id, const char *alias, const char *message,
                                          gboolean on_list, PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb,void *user_data);
#if PHURPLE_INTERNAL_DEBUG
static void phurple_dump_zval(zval *var);
#endif

#ifdef HAVE_SIGNAL_H
static void sighandler(int sig);
static void clean_pid();

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

/**
 * Took this from the libphurples account.c because of need
 * to get the account settings. If the libphurple will change,
 * should fit it.
 */
typedef struct
{
	PurplePrefType type;

	char *ui;

	union
	{
		int integer;
		char *string;
		gboolean boolean;

	} value;

} PurpleAccountSetting;

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
	phurple_write_im_function,              /* write_im             */
	phurple_write_conv_function,            /* write_conv           */
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
	phurple_ui_init,
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
	phurple_request_authorize,				/* request authorize */
	NULL,				/* close account request */
	NULL,
	NULL,
	NULL,
	NULL
};

ZEND_DECLARE_MODULE_GLOBALS(phurple);

void phurple_globals_ctor(zend_phurple_globals *phurple_globals TSRMLS_DC)
{
	ALLOC_INIT_ZVAL(phurple_globals->phurple_client_obj);
	Z_TYPE_P(phurple_globals->phurple_client_obj) = IS_OBJECT;
	
	zend_hash_init(&(phurple_globals->ppos).buddy, 20, NULL, NULL, 0);
	zend_hash_init(&(phurple_globals->ppos).group, 20, NULL, NULL, 0);

	phurple_globals->debug = 0;
	phurple_globals->custom_user_dir = estrdup("/dev/null");
	phurple_globals->custom_plugin_path = estrdup("");
	phurple_globals->ui_id = estrdup("PHP");
}

void phurple_globals_dtor(zend_phurple_globals *phurple_globals TSRMLS_DC) { }

/* True global resources - no need for thread safety here */
static int le_phurple;


/* classes definitions*/
static zend_class_entry *PhurpleClient_ce, *PhurpleConversation_ce, *PhurpleAccount_ce, *PhurpleConnection_ce, *PhurpleBuddy_ce, *PhurpleBuddyList_ce, *PhurpleBuddyGroup_ce;


/* {{{ phurple_functions[] */
zend_function_entry phurple_functions[] = {
	{NULL, NULL, NULL}	/* Must be the last line in phurple_functions[] */
};
/* }}} */

/* {{{ client class methods[] */
zend_function_entry PhurpleClient_methods[] = {
	PHP_ME(PhurpleClient, __construct, NULL, ZEND_ACC_FINAL | ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, getInstance, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleClient, initInternal, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, getCoreVersion, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, writeConv, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, writeIM, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, onSignedOn, NULL, ZEND_ACC_PROTECTED)
	/*PHP_ME(PhurpleClient, onSignedOff, NULL, ZEND_ACC_PROTECTED)*/
	PHP_ME(PhurpleClient, runLoop, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, addAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, getProtocols, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, loopCallback, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, loopHeartBeat, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, deleteAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, findAccount, NULL, ZEND_ACC_PUBLIC )
	PHP_ME(PhurpleClient, authorizeRequest, NULL, ZEND_ACC_PROTECTED)
	PHP_ME(PhurpleClient, iterate, NULL, ZEND_ACC_PUBLIC)
	/*PHP_ME(PhurpleClient, set, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleClient, get, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)*/
	PHP_ME(PhurpleClient, connect, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, disconnect, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleClient, setUserDir, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleClient, setDebug, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleClient, setUiId, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleClient, __clone, NULL, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ conversation class methods[] */
zend_function_entry PhurpleConversation_methods[] = {
	PHP_ME(PhurpleConversation, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleConversation, getName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleConversation, sendIM, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleConversation, getAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleConversation, setAccount, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ account class methods[] */
zend_function_entry PhurpleAccount_methods[] = {
	PHP_ME(PhurpleAccount, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, setPassword, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, setEnabled, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, addBuddy, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, removeBuddy, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, clearSettings, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, set, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, isConnected, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, isConnecting, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, getUserName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, getPassword, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleAccount, get, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ connection class methods[] */
zend_function_entry PhurpleConnection_methods[] = {
	PHP_ME(PhurpleConnection, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleConnection, getAccount, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ buddy class methods[] */
zend_function_entry PhurpleBuddy_methods[] = {
	PHP_ME(PhurpleBuddy, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddy, getName, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddy, getAlias, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddy, getGroup, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddy, getAccount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddy, isOnline, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ buddy list class methods[] */
zend_function_entry PhurpleBuddyList_methods[] = {
	PHP_ME(PhurpleBuddyList, __construct, NULL, ZEND_ACC_PRIVATE)
	PHP_ME(PhurpleBuddyList, addBuddy, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleBuddyList, addGroup, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleBuddyList, findBuddy, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleBuddyList, load, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleBuddyList, findGroup, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleBuddyList, removeBuddy, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	PHP_ME(PhurpleBuddyList, removeGroup, NULL, ZEND_ACC_FINAL | ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ buddy group class methods[] */
zend_function_entry PhurpleBuddyGroup_methods[] = {
	PHP_ME(PhurpleBuddyGroup, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddyGroup, getAccounts, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddyGroup, getSize, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddyGroup, getOnlineCount, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(PhurpleBuddyGroup, getName, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ phurple_module_entry */
zend_module_entry phurple_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"phurple",
	phurple_functions,
	PHP_MINIT(phurple),
	PHP_MSHUTDOWN(phurple),
	PHP_RINIT(phurple),
	PHP_RSHUTDOWN(phurple),
	PHP_MINFO(phurple),
#if ZEND_MODULE_API_NO >= 20010901
	"0.4",
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PHURPLE
ZEND_GET_MODULE(phurple)
#endif

/* {{{ PHP_INI */
PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("phurple.custom_plugin_path", "", PHP_INI_ALL, OnUpdateString, custom_plugin_path, zend_phurple_globals, phurple_globals)
PHP_INI_END()
/* }}} */


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(phurple)
{
#ifdef ZTS
	ts_allocate_id(&phurple_globals_id,
			sizeof(zend_phurple_globals),
			(ts_allocate_ctor)phurple_globals_ctor,
			(ts_allocate_dtor)phurple_globals_dtor);
#else
	phurple_globals_ctor(&phurple_globals TSRMLS_CC);
#endif
	
	REGISTER_INI_ENTRIES();

	g_log_set_handler (NULL, G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL | G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_RECURSION, phurple_g_log_handler, NULL);
	
	/* initalizing classes */
	zend_class_entry ce;
	
	/* classes definitions */

#if USING_PHP_53
#define PHURPLE_CLIENT_CLASS_NAME "Client"
	INIT_CLASS_ENTRY(ce, "Phurple::Client", PhurpleClient_methods);
#else
#define PHURPLE_CLIENT_CLASS_NAME "PhurpleClient"
	INIT_CLASS_ENTRY(ce, PHURPLE_CLIENT_CLASS_NAME, PhurpleClient_methods);
#endif
	PhurpleClient_ce = zend_register_internal_class(&ce TSRMLS_CC);

	/* A type of conversation */
	zend_declare_class_constant_long(PhurpleClient_ce, "CONV_TYPE_UNKNOWN", sizeof("CONV_TYPE_UNKNOWN")-1, PURPLE_CONV_TYPE_UNKNOWN TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "CONV_TYPE_IM", sizeof("CONV_TYPE_IM")-1, PURPLE_CONV_TYPE_IM TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "CONV_TYPE_CHAT", sizeof("CONV_TYPE_CHAT")-1, PURPLE_CONV_TYPE_CHAT TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "CONV_TYPE_MISC", sizeof("CONV_TYPE_MISC")-1, PURPLE_CONV_TYPE_MISC TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "CONV_TYPE_ANY", sizeof("CONV_TYPE_ANY")-1, PURPLE_CONV_TYPE_ANY TSRMLS_CC);
	/* Flags applicable to a message */
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_SEND", sizeof("MESSAGE_SEND")-1, PURPLE_MESSAGE_SEND TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_RECV", sizeof("MESSAGE_RECV")-1, PURPLE_MESSAGE_RECV TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_SYSTEM", sizeof("MESSAGE_SYSTEM")-1, PURPLE_MESSAGE_SYSTEM TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_AUTO_RESP", sizeof("MESSAGE_AUTO_RESP")-1, PURPLE_MESSAGE_AUTO_RESP TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_ACTIVE_ONLY", sizeof("MESSAGE_ACTIVE_ONLY")-1, PURPLE_MESSAGE_ACTIVE_ONLY TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_NICK", sizeof("MESSAGE_NICK")-1, PURPLE_MESSAGE_NICK TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_NO_LOG", sizeof("MESSAGE_NO_LOG")-1, PURPLE_MESSAGE_NO_LOG TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_WHISPER", sizeof("MESSAGE_WHISPER")-1, PURPLE_MESSAGE_WHISPER TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_ERROR", sizeof("MESSAGE_ERROR")-1, PURPLE_MESSAGE_ERROR TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_DELAYED", sizeof("MESSAGE_DELAYED")-1, PURPLE_MESSAGE_DELAYED TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_RAW", sizeof("MESSAGE_RAW")-1, PURPLE_MESSAGE_RAW TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_IMAGES", sizeof("MESSAGE_IMAGES")-1, PURPLE_MESSAGE_IMAGES TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_NOTIFY", sizeof("MESSAGE_NOTIFY")-1, PURPLE_MESSAGE_NOTIFY TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_NO_LINKIFY", sizeof("MESSAGE_NO_LINKIFY")-1, PURPLE_MESSAGE_NO_LINKIFY TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "MESSAGE_INVISIBLE", sizeof("MESSAGE_INVISIBLE")-1, PURPLE_MESSAGE_INVISIBLE TSRMLS_CC);
	/* Flags applicable to a status */
	zend_declare_class_constant_long(PhurpleClient_ce, "STATUS_OFFLINE", sizeof("STATUS_OFFLINE")-1, PURPLE_STATUS_OFFLINE TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "STATUS_AVAILABLE", sizeof("STATUS_AVAILABLE")-1, PURPLE_STATUS_AVAILABLE TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "STATUS_UNAVAILABLE", sizeof("STATUS_UNAVAILABLE")-1, PURPLE_STATUS_UNAVAILABLE TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "STATUS_INVISIBLE", sizeof("STATUS_INVISIBLE")-1, PURPLE_STATUS_INVISIBLE TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "STATUS_AWAY", sizeof("STATUS_AWAY")-1, PURPLE_STATUS_AWAY TSRMLS_CC);
	zend_declare_class_constant_long(PhurpleClient_ce, "STATUS_MOBILE", sizeof("STATUS_MOBILE")-1, PURPLE_STATUS_MOBILE TSRMLS_CC);
	
#if USING_PHP_53
#define PHURPLE_CONVERSATION_CLASS_NAME "Conversation"
	INIT_CLASS_ENTRY(ce, "Phurple::Conversation", PhurpleConversation_methods);
#else
#define PHURPLE_CONVERSATION_CLASS_NAME "PhurpleConversation"
	INIT_CLASS_ENTRY(ce, PHURPLE_CONVERSATION_CLASS_NAME, PhurpleConversation_methods);
#endif
	PhurpleConversation_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PhurpleConversation_ce, "index", sizeof("index")-1, -1, ZEND_ACC_PRIVATE TSRMLS_CC);

#if USING_PHP_53
#define PHURPLE_ACCOUNT_CLASS_NAME "Account"
	INIT_CLASS_ENTRY(ce, "Phurple::Account", PhurpleAccount_methods);
#else
#define PHURPLE_ACCOUNT_CLASS_NAME "PhurpleAccount"
	INIT_CLASS_ENTRY(ce, PHURPLE_ACCOUNT_CLASS_NAME, PhurpleAccount_methods);
#endif
	PhurpleAccount_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PhurpleAccount_ce, "index", sizeof("index")-1, -1, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE TSRMLS_CC);
	
#if USING_PHP_53
#define PHURPLE_CONNECION_CLASS_NAME "Connection"
	INIT_CLASS_ENTRY(ce, "Phurple::Connection", PhurpleConnection_methods);
#else
#define PHURPLE_CONNECION_CLASS_NAME "PhurpleConnection"
	INIT_CLASS_ENTRY(ce, PHURPLE_CONNECION_CLASS_NAME, PhurpleConnection_methods);
#endif
	PhurpleConnection_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PhurpleConnection_ce, "index", sizeof("index")-1, -1, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE TSRMLS_CC);

#if USING_PHP_53
#define PHURPLE_BUDDY_CLASS_NAME "Buddy"
	INIT_CLASS_ENTRY(ce, "Phurple::Buddy", PhurpleBuddy_methods);
#else
#define PHURPLE_BUDDY_CLASS_NAME "PhurpleBuddy"
	INIT_CLASS_ENTRY(ce, PHURPLE_BUDDY_CLASS_NAME, PhurpleBuddy_methods);
#endif
	PhurpleBuddy_ce = zend_register_internal_class(&ce TSRMLS_CC);
	zend_declare_property_long(PhurpleBuddy_ce, "index", sizeof("index")-1, -1, ZEND_ACC_FINAL | ZEND_ACC_PRIVATE TSRMLS_CC);

#if USING_PHP_53
#define PHURPLE_BUDDYLIST_CLASS_NAME "BuddyList"
	INIT_CLASS_ENTRY(ce, "Phurple::BuddyList", PhurpleBuddyList_methods);
#else
#define PHURPLE_BUDDYLIST_CLASS_NAME "PhurpleBuddyList"
	INIT_CLASS_ENTRY(ce, PHURPLE_BUDDYLIST_CLASS_NAME, PhurpleBuddyList_methods);
#endif
	PhurpleBuddyList_ce = zend_register_internal_class(&ce TSRMLS_CC);

#if USING_PHP_53
#define PHURPLE_BUDDY_GROUP_CLASS_NAME "Phurple::BuddyGroup"
	INIT_CLASS_ENTRY(ce, "Phurple::BuddyGroup", PhurpleBuddyGroup_methods);
#else
#define PHURPLE_BUDDY_GROUP_CLASS_NAME "PhurpleBuddyGroup"
	INIT_CLASS_ENTRY(ce, PHURPLE_BUDDY_GROUP_CLASS_NAME, PhurpleBuddyGroup_methods);
#endif
	PhurpleBuddyGroup_ce = zend_register_internal_class(&ce TSRMLS_CC);

	
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
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(phurple)
{
	UNREGISTER_INI_ENTRIES();

#ifdef ZTS
	ts_free_id(phurple_globals_id);
#else
	phurple_globals_dtor(&phurple_globals TSRMLS_CC);
#endif
	
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(phurple)
{
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(phurple)
{
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(phurple)
{
/*	php_info_print_table_start();
	php_info_print_table_header(2, "phurple support", "enabled");
	php_info_print_table_end();
*/
	
	DISPLAY_INI_ENTRIES();

}
/* }}} */


/*
**
**
** Phurple client methods
**
*/

/* {{{ */
PHP_METHOD(PhurpleClient, __construct)
{

}
/* }}} */


/* {{{ proto void PhurpleClient::runLoop(int interval)
	Creates the main loop*/
PHP_METHOD(PhurpleClient, runLoop)
{
	long interval = 0;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &interval) == FAILURE) {
		RETURN_NULL();
	}
	
	phurple_g_loop_callback();
	
	if(interval) {
		g_timeout_add(interval, (GSourceFunc)phurple_heartbeat_callback, NULL);
	}
	
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
}
/* }}} */


/* {{{ proto object PhurpleClient::addAccount(string dsn)
	adds a new account to the current client*/
PHP_METHOD(PhurpleClient, addAccount)
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

	re = pcre_compile("([a-zA-Z-]+)://([^:]+):?([^@]*)@?([a-zA-Z0-9-.]*):?([0-9]*)", 0, &error, &erroffset, NULL);

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

	account = purple_account_new(estrdup(nick), phurple_get_protocol_id_by_name(protocol));

	if(NULL != account) {

		purple_account_set_password(account, estrdup(password));

		if(strlen(host)) {
			purple_account_set_string(account, "server", host);
		}

		if(strlen(port) && atoi(port)) {
			purple_account_set_int(account, "port", (int)atoi(port));
		}

		purple_account_set_enabled(account, PHURPLE_G(ui_id), 1);

		purple_accounts_add(account);

		accounts = purple_accounts_get_all();

		ZVAL_NULL(return_value);
		Z_TYPE_P(return_value) = IS_OBJECT;
		object_init_ex(return_value, PhurpleAccount_ce);
		zend_update_property_long(PhurpleAccount_ce,
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


/* {{{ proto void PhurpleClient::deleteAccount(PhurpleAccount account)
	Removes an account from the list of accounts*/
PHP_METHOD(PhurpleClient, deleteAccount)
{
	zval *account;
	PurpleAccount *paccount = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &account) == FAILURE) {
		RETURN_FALSE;
	}

	switch (Z_TYPE_P(account)) {
		case IS_OBJECT:
			paccount = g_list_nth_data(purple_accounts_get_all(), (guint)Z_LVAL_P(zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));
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


/* {{{ proto PhurpleAccount PhurpleClient::findAccount(string name)
	Finds the specified account in the accounts list */
PHP_METHOD(PhurpleClient, findAccount)
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
		object_init_ex(return_value, PhurpleAccount_ce);
		zend_update_property_long(PhurpleAccount_ce,
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


/* {{{ proto string PhurpleClient::getCoreVersion(void)
	Returns the libphurple core version string */
PHP_METHOD(PhurpleClient, getCoreVersion)
{	
	char *version = estrdup(purple_core_get_version());

	RETURN_STRING(version, 0);
}
/* }}} */


/* {{{ proto object PhurpleClient::getInstance(void)
	creates new PhurpleClient instance*/
PHP_METHOD(PhurpleClient, getInstance)
{
	if(NULL == zend_objects_get_address(PHURPLE_G(phurple_client_obj) TSRMLS_CC)) {

		/**
		 * phurple initialization stuff
		 */
		purple_util_set_user_dir(PHURPLE_G(custom_user_dir));
		purple_debug_set_enabled(PHURPLE_G(debug));
		purple_core_set_ui_ops(&php_core_uiops);
		purple_accounts_set_ui_ops(&php_account_uiops);
		purple_eventloop_set_ui_ops(&glib_eventloops);
		purple_plugins_add_search_path(INI_STR("phurple.custom_plugin_path"));
	
		if (!purple_core_init(PHURPLE_G(ui_id))) {
#ifdef HAVE_SIGNAL_H
			g_free(segfault_message);
#endif
			zend_throw_exception(NULL, "Couldn't initalize the libphurple core", 0 TSRMLS_CC);
			RETURN_NULL();
		}
	
		purple_set_blist(purple_blist_new());
		purple_blist_load();
		
		purple_prefs_load();

		PurpleSavedStatus *saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
		purple_savedstatus_activate(saved_status);


		MAKE_STD_ZVAL(PHURPLE_G(phurple_client_obj));
		Z_TYPE_P(PHURPLE_G(phurple_client_obj)) = IS_OBJECT;
#if USING_PHP_53
		object_init_ex(PHURPLE_G(phurple_client_obj), EG(called_scope));
#else
		zend_class_entry **ce = NULL;
		zend_hash_find(EG(class_table), "customphurpleclient", sizeof("customphurpleclient"), (void **) &ce);

		if(ce && (*ce)->parent && 0 == strcmp(PHURPLE_CLIENT_CLASS_NAME, (*ce)->parent->name)) {
			object_init_ex(PHURPLE_G(phurple_client_obj), *ce);
		} else {
			zend_throw_exception(NULL,
			                     "The "
			                     PHURPLE_CLIENT_CLASS_NAME
			                     " child class must be named CustomPhurpleClient for PHP < v5.3",
			                     0 TSRMLS_CC);
			return;
		}
		/* object_init_ex(tmp, EG(current_execute_data->fbc->common.scope)); would be beautiful but works not as expected */
		
#endif
		*return_value = *PHURPLE_G(phurple_client_obj);

		call_custom_method(&PHURPLE_G(phurple_client_obj),
		                   Z_OBJCE_P(PHURPLE_G(phurple_client_obj)),
		                   NULL,
		                   "initinternal",
		                   sizeof("initinternal")-1,
		                   NULL,
		                  0);

		return;
	}

	*return_value = *PHURPLE_G(phurple_client_obj);
	
	return;
}
/* }}} */


/* {{{ proto array PhurpleClient::getProtocols(void)
	Returns a list of all valid protocol plugins */
PHP_METHOD(PhurpleClient, getProtocols)
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


/* {{{ proto void PhurpleClient::setUserDir([string $userDir])
	Define a custom phurple settings directory, overriding the default (user's home directory/.phurple) */
PHP_METHOD(PhurpleClient, setUserDir) {
	char *user_dir;
	int user_dir_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user_dir, &user_dir_len) == FAILURE) {
		return;
	}

	PHURPLE_G(custom_user_dir) = estrdup(user_dir);
	
	purple_util_set_user_dir(user_dir);
}
/* }}} */


PHP_METHOD(PhurpleClient, setDebug)
{
	zval *debug;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &debug) == FAILURE) {
		return;
	}

	if(Z_TYPE_P(debug) == IS_BOOL) {
		PHURPLE_G(debug) = Z_BVAL_P(debug) ? 1 : 0;
	} else if(Z_TYPE_P(debug) == IS_LONG) {
		PHURPLE_G(debug) = Z_LVAL_P(debug) == 0 ? 0 : 1;
	} else if(Z_TYPE_P(debug) == IS_DOUBLE) {
		PHURPLE_G(debug) = Z_DVAL_P(debug) == 0 ? 0 : 1;
	}
}


PHP_METHOD(PhurpleClient, setUiId)
{
	char *ui_id;
	int ui_id_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ui_id, &ui_id_len) == FAILURE) {
		return;
	}

	PHURPLE_G(ui_id) = estrdup(ui_id);
}


/* {{{ proto boolean PhurpleClient::iterate(void)
	Do a single glibs main loop iteration
*/
PHP_METHOD(PhurpleClient, iterate)
{
	RETVAL_BOOL(g_main_context_iteration(NULL, 0));
}
/* }}} */


/* {{{ */
/*PHP_METHOD(PhurpleClient, set)
{

}*/
/* }}} */


/* {{{ */
/*PHP_METHOD(PhurpleClient, get)
{

}*/
/* }}} */


/* {{{ proto void PhurpleClient::connect()
	Connect the client*/
PHP_METHOD(PhurpleClient, connect)
{
	purple_signal_connect(purple_connections_get_handle(),
	                      estrdup(SIGNAL_SIGNED_ON),
	                      &PHURPLE_G(connection_handle),
	                      PURPLE_CALLBACK(phurple_signed_on_function),
	                      NULL
	                      );
}
/* }}} */


/* {{{ proto void PhurpleClient::disconnect()
	Close all client connections*/
PHP_METHOD(PhurpleClient, disconnect)
{
	GList *iter = purple_accounts_get_all();

	for (; iter; iter = iter->next)
	{
		PurpleAccount *account = iter->data;
		purple_account_disconnect(account);
	}

	purple_signal_connect(purple_connections_get_handle(),
	                      SIGNAL_SIGNED_OFF,
	                      &PHURPLE_G(connection_handle),
	                      PURPLE_CALLBACK(phurple_signed_off_function),
	                      NULL
	                      );
}
/* }}} */


/* {{{ proto PhurpleClient PhurpleClient::__clone()
	Clone method block, because it's private final*/
PHP_METHOD(PhurpleClient, __clone)
{

}
/* }}} */

/*
**
**
** End phurple client methods
**
*/


/*
**
**
** Phurple client callback methods
**
*/

/* {{{ proto void PhurpleClient::writeConv(PhurpleConversation conversation, PhurplePuddy buddy, string message, int flags, timestamp time)
	This callback method writes to the conversation, if implemented*/
PHP_METHOD(PhurpleClient, writeConv)
{
}
/* }}} */


/* {{{ proto void PhurpleClient::writeIM(PhurpleConversation conversation, PhurplePuddy buddy, string message, int flags, timestamp time)
	This callback method writes to the conversation, if implemented*/
PHP_METHOD(PhurpleClient, writeIM)
{
}
/* }}} */

/* {{{ proto void PhurpleClient::onSignedOn(PhurpleConnection connection)
	This callback is called at the moment, where the client got singed on, if implemented */
PHP_METHOD(PhurpleClient, onSignedOn)
{
}
/* }}} */


/* {{{ proto void PhurpleClient::initInternal(void)
	This callback method is called within the PhurpleClient::getInstance, so if implemented, can initalize some internal stuff*/
PHP_METHOD(PhurpleClient, initInternal)
{
}
/* }}}*/


/* {{{ proto void PhurpleClient::loopCallback(void)
	This callback method is called within the PhurpleClient::runLoop */
PHP_METHOD(PhurpleClient, loopCallback)
{
}
/* }}} */


/* {{{ proto void PhurpleClient::loopHeartBeat(void) 
	This callback method is invoked by glib timer */
PHP_METHOD(PhurpleClient, loopHeartBeat)
{
}
/* }}} */


/* {{{ proto boolean PhurpleClient::authorizeRequest(PhurpleAccount account, string $remote_user, string $message, boolean $on_list)
	This callback method is invoked, when someone adds us to his buddy list */
PHP_METHOD(PhurpleClient, authorizeRequest)
{
	
}
/* }}} */


/* {{{ */
/*PHP_METHOD(PhurpleClient, onSignedOff)
{

}*/
/* }}} */


/*
**
**
** End phurple client callback methods
**
*/


/*
**
**
** Phurple connection methods
**
*/

/* {{{ proto object PhurpleConnection::__construct()
	constructor*/
PHP_METHOD(PhurpleConnection, __construct)
{
	
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleConnection::getAccount()
		Returns the connection's account*/
PHP_METHOD(PhurpleConnection, getAccount)
{
	PurpleConnection *conn = NULL;
	PurpleAccount *acc = NULL;
	GList *accounts = NULL;

	conn = g_list_nth_data (purple_connections_get_all(), (guint)Z_LVAL_P(zend_read_property(PhurpleConnection_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	if(NULL != conn) {
		acc = purple_connection_get_account(conn);
		if(NULL != acc) {
			accounts = purple_accounts_get_all();

			ZVAL_NULL(return_value);
			Z_TYPE_P(return_value) = IS_OBJECT;
			object_init_ex(return_value, PhurpleAccount_ce);
			zend_update_property_long(PhurpleAccount_ce,
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
** End phurple connection methods
**
*/


/*
**
**
** Phurple account methods
**
*/

/* {{{ proto object PhurpleAccount::__construct(string user_name, string protocol_name)
	Creates a new account*/
PHP_METHOD(PhurpleAccount, __construct)
{
	char *username, *protocol_name, *protocol_id;
	int username_len, protocol_name_len;
	GList *iter, *accounts;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &username, &username_len, &protocol_name, &protocol_name_len) == FAILURE) {
		RETURN_NULL();
	}

	account = purple_account_new(estrdup(username), phurple_get_protocol_id_by_name(protocol_name));
	purple_accounts_add(account);
	if(NULL != account) {
		accounts = purple_accounts_get_all();

		zend_update_property_long(PhurpleAccount_ce,
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


/* {{{ proto void PhurpleAccount::setPassword(int account, string password)
	Sets the account's password */
PHP_METHOD(PhurpleAccount, setPassword)
{
	int password_len;
	char *password;
	PurpleAccount *account = NULL;
	zval *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));

	/*php_printf("account_index = %d\n", Z_LVAL_P(account_index));*/

 	account = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != account) {
		purple_account_set_password(account, estrdup(password));
	}
}
/* }}} */


/* {{{ proto void PhurpleAccount::setEnabled(bool enabled)
	Sets whether or not this account is enabled for some UI */
PHP_METHOD(PhurpleAccount, setEnabled)
{
	zend_bool enabled;
	PurpleAccount *account = NULL;
	zval *account_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &enabled) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	account = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != account) {
		purple_account_set_enabled(account, PHURPLE_G(ui_id), (gboolean) enabled);
	}
}
/* }}} */


/* {{{ proto bool PhurpleAccount::addBuddy(PhurpleBuddy buddy)
	Adds a buddy to the server-side buddy list for the specified account */
PHP_METHOD(PhurpleAccount, addBuddy)
{
	PurpleAccount *paccount = NULL;
	PurpleBuddy *pbuddy = NULL;
	zval *account_index, *buddy_index, *buddy;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PhurpleBuddy_ce) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(paccount) {
		struct phurple_object_storage *pp = &PHURPLE_G(ppos);

		buddy_index = zend_read_property(PhurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
		zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(buddy_index), (void**)&pbuddy);

		if(pbuddy) {
			purple_blist_add_buddy(pbuddy, NULL, NULL, NULL);
			purple_account_add_buddy(paccount, pbuddy);
			RETURN_TRUE;
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool PhurpleAccount::removeBuddy(PhurpleBuddy buddy)
	Removes a buddy from the server-side buddy list for the specified account */
PHP_METHOD(PhurpleAccount, removeBuddy)
{
	PurpleAccount *paccount = NULL;
	PurpleBuddy *pbuddy = NULL;
	zval *account_index, *buddy_index, *buddy;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PhurpleBuddy_ce) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(account_index);
	ZVAL_LONG(account_index, Z_LVAL_P(zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));
	if(NULL != paccount) {
		struct phurple_object_storage *pp = &PHURPLE_G(ppos);

		buddy_index = zend_read_property(PhurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
		zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(buddy_index), (void**)&pbuddy);

		if(pbuddy) {
			purple_account_remove_buddy(paccount, pbuddy, purple_buddy_get_group(pbuddy));
			RETURN_TRUE;
		}
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto void PhurpleAccount::clearSettings(void)
	Clears all protocol-specific settings on an account. }}} */
PHP_METHOD(PhurpleAccount, clearSettings)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		purple_account_clear_settings(paccount);
		RETURN_TRUE;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto void PhurpleAccount::set(string name, string value)
	Sets a protocol-specific setting for an account.
	The value types expected are int, string or bool. */
PHP_METHOD(PhurpleAccount, set)
{
	PurpleAccount *paccount = NULL;
	zval *index, *value;
	char *name;
	int name_len;
	
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &value) == FAILURE) {
		RETURN_FALSE;
	}
	
	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		switch (Z_TYPE_P(value)) {
			case IS_BOOL:
				purple_account_set_ui_bool (paccount, PHURPLE_G(ui_id), name, (gboolean) Z_LVAL_P(value));
			break;
			
			case IS_LONG:
			case IS_DOUBLE:
				purple_account_set_ui_int (paccount, PHURPLE_G(ui_id), name, (int) Z_LVAL_P(value));
			break;
				
			case IS_STRING:
				purple_account_set_ui_string (paccount, PHURPLE_G(ui_id), name, Z_STRVAL_P(value));
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


/* {{{ proto mixed PhurpleAccount::get($key)
	Sets a protocol-specific setting for an account.
	Possible return datatypes are int|boolean|string or null 
	if the setting isn't set or not found*/
PHP_METHOD(PhurpleAccount, get)
{
	PurpleAccount *paccount = NULL;
	PurpleAccountSetting *setting;
	GHashTable *table;
	zval *index;
	char *name;
	int name_len;
	
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);

	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));

	if(paccount) {

		if((table = g_hash_table_lookup(paccount->ui_settings, PHURPLE_G(ui_id))) == NULL) {
			RETURN_NULL();
		}

		setting = g_hash_table_lookup(table, name);
		if(setting) {
			switch(setting->type) {
				case PURPLE_PREF_BOOLEAN:
					RETVAL_BOOL(setting->value.boolean);
					return;
				break;

				case PURPLE_PREF_INT:
					RETVAL_LONG(setting->value.integer);
					return;
				break;

				case PURPLE_PREF_STRING:
					RETVAL_STRING(setting->value.string, 1);
					return;
				break;

				default:
					RETURN_NULL();
				break;
			}
		}
		RETURN_NULL();
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto boolean PhurpleAccount::isConnected(void)
	Returns whether or not the account is connected*/
PHP_METHOD(PhurpleAccount, isConnected)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETVAL_BOOL((long) purple_account_is_connected(paccount));
		return;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto boolean PhurpleAccount::isConnecting(void)
	Returns whether or not the account is connecting*/
PHP_METHOD(PhurpleAccount, isConnecting)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETVAL_BOOL((long) purple_account_is_connecting(paccount));
		return;
	}
	
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto string PhurpleAccount::getUserName(void) Returns the account's username */
PHP_METHOD(PhurpleAccount, getUserName)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));
	
	if(paccount) {
		RETURN_STRING(estrdup(purple_account_get_username(paccount)), 0);
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PhurpleAccount::getPassword(void) Returns the account's password */
PHP_METHOD(PhurpleAccount, getPassword)
{
	PurpleAccount *paccount = NULL;
	zval *index;
	
	index = zend_read_property(PhurpleAccount_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	
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
** End phurple account methods
**
*/


/*
**
**
** Phurple pounce methods
**
*/

/*
**
**
** End phurple pounce methods
**
*/


/*
**
**
** Phurple conversation methods
**
*/

/* {{{ proto int PhurpleConversation::__construct(int type, PhurpleAccount account, string name)
	Creates a new conversation of the specified type */
PHP_METHOD(PhurpleConversation, __construct)
{
	int type, name_len, conv_list_position = -1;
	char *name;
	PurpleConversation *conv = NULL;
	PurpleAccount *paccount = NULL;
	GList *conversations = NULL, *cnv;
	zval *account, *account_index;
	gchar *name1;
	const gchar *name2;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lOs", &type, &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	account_index = zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC);
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));

	if(NULL != account) {
		conv = purple_conversation_new(type, paccount, estrdup(name));
		conversations = purple_get_conversations();
/*
		zend_update_property_long(PhurpleConversation_ce,
		                          getThis(),
		                          "index",
		                          sizeof("index")-1,
		                          (long)g_list_position(conversations, g_list_last(conversations)) TSRMLS_CC
		                          );*/
		cnv = conversations;
		name1 = g_strdup(purple_normalize(paccount, name));

		for (; cnv != NULL; cnv = cnv->next) {
// 			if((PurpleConversation *)cnv->data == conv) {
// 				conv_list_position = g_list_position(conversations, cnv);
// 			}
			name2 = purple_normalize(paccount, purple_conversation_get_name((PurpleConversation *)cnv->data));

			if ((paccount == purple_conversation_get_account((PurpleConversation *)cnv->data)) &&
					!purple_utf8_strcasecmp(name1, name2)) {
				conv_list_position = g_list_position(conversations, cnv);
			}
		}

		conv_list_position = conv_list_position == -1
		                     ? g_list_position(conversations, g_list_last(conversations))
		                     : conv_list_position;

		zend_update_property_long(PhurpleConversation_ce,
		                          getThis(),
		                          "index",
		                          sizeof("index")-1,
		                          (long)conv_list_position TSRMLS_CC
		                          );

		return;
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PhurpleConversation::getName(void)
	Returns the specified conversation's name*/
PHP_METHOD(PhurpleConversation, getName)
{
	zval *conversation_index;
	PurpleConversation *conversation = NULL;

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		RETURN_STRING(estrdup(purple_conversation_get_name(conversation)), 0);
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto void PhurpleConversation::sendIM(string message)
	Sends a message to this IM conversation */
PHP_METHOD(PhurpleConversation, sendIM)
{
	int message_len;
	char *message;
	PurpleConversation *conversation = NULL;
	zval *conversation_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &message, &message_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		purple_conv_im_send(PURPLE_CONV_IM(conversation), estrdup(message));
	}
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleConversation::getAccount(void)
	Gets the account of this conversation*/
PHP_METHOD(PhurpleConversation, getAccount)
{
	PurpleConversation *conversation = NULL;
	PurpleAccount *acc = NULL;
	zval *conversation_index;

	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	if(NULL != conversation) {
		acc = purple_conversation_get_account(conversation);
		if(NULL != acc) {
			ZVAL_NULL(return_value);
			Z_TYPE_P(return_value) = IS_OBJECT;
			object_init_ex(return_value, PhurpleAccount_ce);
			zend_update_property_long(PhurpleAccount_ce,
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

/* {{{ proto void PhurpleConversation::setAccount(PhurpleAccount account)
	Sets the specified conversation's phurple_account */
PHP_METHOD(PhurpleConversation, setAccount)
{
	PurpleConversation *pconv = NULL;
	PurpleAccount *paccount = NULL;
	zval *account;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &account, PhurpleAccount_ce) == FAILURE) {
		RETURN_NULL();
	}
	
	pconv = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	if(pconv) {
		paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));
		
		if(account) {
			purple_conversation_set_account(pconv, paccount);
		}
	}
}
/* }}} */

/*
**
**
** End phurple conversation methods
**
*/


/*
**
**
** Phurple Buddy methods
**
*/

/* {{{ proto object PhurpleBuddy::__construct(PhurpleAccount account, string name, string alias)
	Creates new buddy*/
PHP_METHOD(PhurpleBuddy, __construct)
{
	PurpleAccount *paccount = NULL;
	PurpleBuddy *pbuddy = NULL;
	char *name, *alias = "";
	int name_len, alias_len = 0, account_index;
	zval *account;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|s", &account, PhurpleAccount_ce, &name, &name_len, &alias, &alias_len) == FAILURE) {
		RETURN_NULL();
	}

	paccount = g_list_nth_data (purple_accounts_get_all(), Z_LVAL_P(zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));

	if(paccount) {
		pbuddy = purple_find_buddy(paccount, name);
		struct phurple_object_storage *pp = &PHURPLE_G(ppos);

		if(pbuddy) {

			int ind = phurple_hash_index_find(&pp->buddy, pbuddy);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(PhurpleBuddy_ce,
				                          getThis(),
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PhurpleBuddy_ce,
				                          getThis(),
				                          "index",
				                          sizeof("index")-1,
				                          (long)ind TSRMLS_CC
				                          );
			}

			return;
		} else {
			pbuddy = purple_buddy_new(paccount, name, alias_len ? alias : name);
			ulong nextid = zend_hash_next_free_element(&pp->buddy);
			zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
			zend_update_property_long(PhurpleBuddy_ce,
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


/* {{{ proto string PhurpleBuddy::getName(void)
	Gets buddy name*/
PHP_METHOD(PhurpleBuddy, getName)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;

	index = zend_read_property(PhurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PHURPLE_G(ppos).buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		const char *name = purple_buddy_get_name(pbuddy);
		if(name) {
			RETURN_STRING(estrdup(name), 0);
		}
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PhurpleBuddy::getAlias(void)
	gets buddy alias */
PHP_METHOD(PhurpleBuddy, getAlias)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;

	index = zend_read_property(PhurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PHURPLE_G(ppos).buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		char const *alias = purple_buddy_get_alias_only(pbuddy);
		RETURN_STRING( alias && *alias ? estrdup(alias) : "", 0);
	}
	
	RETURN_NULL();
	
}
/* }}} */


/* {{{ proto PhurpleGroup PhurpleBuddy::getGroup(void)
	gets buddy's group */
PHP_METHOD(PhurpleBuddy, getGroup)
{
	zval *index, *tmp;
	PurpleBuddy *pbuddy = NULL;
	PurpleGroup *pgroup = NULL;
	struct phurple_object_storage *pp = &PHURPLE_G(ppos);
			
	index = zend_read_property(PhurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		PHURPLE_MK_OBJ(tmp, PhurpleBuddyGroup_ce);

		pgroup = purple_buddy_get_group(pbuddy);
		if(pgroup) {
			int ind = phurple_hash_index_find(&pp->group, pgroup);
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->group);
				zend_hash_index_update(&pp->group, nextid, pgroup, sizeof(PurpleGroup), NULL);
				zend_update_property_long(PhurpleBuddyGroup_ce,
				                          tmp,
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PhurpleBuddyGroup_ce,
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


/* {{{ proto PhurpleAccount PhurpleBuddy::getAccount(void)
	gets buddy's account */
PHP_METHOD(PhurpleBuddy, getAccount)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;
	PurpleAccount *paccount = NULL;
	struct phurple_object_storage *pp = &PHURPLE_G(ppos);
	GList *accounts = NULL;
			
	index = zend_read_property(PhurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	if(pbuddy) {
		PHURPLE_MK_OBJ(return_value, PhurpleAccount_ce);

		paccount = purple_buddy_get_account(pbuddy);
		if(paccount) {
			accounts = purple_accounts_get_all();

			zend_update_property_long(PhurpleAccount_ce,
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



/* {{{ proto bool PhurpleBuddy::isOnline(void)
	checks weither the buddy is online */
PHP_METHOD(PhurpleBuddy, isOnline)
{
	zval *index;
	PurpleBuddy *pbuddy = NULL;

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);
			
	index = zend_read_property(PhurpleBuddy_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	RETVAL_BOOL(PURPLE_BUDDY_IS_ONLINE(pbuddy));
}
/* }}} */

/*
**
**
** End phurple Buddy methods
**
*/


/*
**
**
** Phurple BuddyList methods
**
*/

/* {{{ proto PhurpleBuddyList PhurpleBuddyList::__construct(void)
	should newer be called*/
PHP_METHOD(PhurpleBuddyList, __construct)
{
}
/* }}} */


/* {{{ proto bool PhurpleBuddyList::addBuddy(PhurpleBuddy buddy[, PhurpleGroup group])
	adds the buddy to the blist (optionally to the given group in the blist, not implemented yet)*/
PHP_METHOD(PhurpleBuddyList, addBuddy)
{
	zval *buddy, *group, *index;
	PurpleBuddy *pbuddy = NULL;
	PurpleGroup *pgroup = NULL;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|O", &buddy, PhurpleBuddy_ce, &group, PhurpleBuddyGroup_ce) == FAILURE) {
		RETURN_NULL();
	}

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);

	index = zend_read_property(PhurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->buddy, (ulong)Z_LVAL_P(index), (void**)&pbuddy);

	index = zend_read_property(PhurpleBuddyGroup_ce, group, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->group, (ulong)Z_LVAL_P(index), (void**)&pgroup);
	
	if(pbuddy && pgroup) {
		purple_blist_add_buddy(pbuddy, NULL, pgroup, NULL);
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto bool PhurpleBuddyList::addGroup(string name)
	Adds new group to the blist */
PHP_METHOD(PhurpleBuddyList, addGroup)
{
	zval *group, *index;
	PurpleGroup *pgroup;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PhurpleBuddyGroup_ce) == FAILURE) {
		RETURN_NULL();
	}

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);
	
	index = zend_read_property(PhurpleBuddyGroup_ce, group, "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&pp->group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
		purple_blist_add_group(pgroup, NULL);
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */


/* {{{ proto PhurpleBuddy PhurpleBuddyList::findBuddy(PhurpleAccount account, string name)
	returns the buddy, if found */
PHP_METHOD(PhurpleBuddyList, findBuddy)
{
	zval *account, *index, *buddy;
	char *name;
	int name_len;
	PurpleBuddy *pbuddy;
	PurpleAccount *paccount;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os", &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);

	index = zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC);
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(index));

	if(paccount) {
		pbuddy = purple_find_buddy(paccount, name);

		if(pbuddy) {
			int ind = phurple_hash_index_find(&pp->buddy, pbuddy);
			PHURPLE_MK_OBJ(buddy, PhurpleBuddy_ce);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(PhurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PhurpleBuddy_ce,
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


/* {{{ proto void PhurpleBuddyList::load(void)
	loads the blist.xml from the homedir */
PHP_METHOD(PhurpleBuddyList, load)
{/*
	purple_blist_load();

	purple_set_blist(purple_get_blist());*/
/* dead method, do nothing here*/
}
/* }}} */


/* {{{ proto PhurpleGroup PhurpleBuddyList::findGroup(string group)
	Finds group by name }}} */
PHP_METHOD(PhurpleBuddyList, findGroup)
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
		
		object_init_ex(return_value, PhurpleBuddyGroup_ce);
		
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
		fcc.function_handler = PhurpleBuddyGroup_ce->constructor;
		fcc.calling_scope = EG(scope);
		fcc.object_pp = &return_value;
		
		if (zend_call_function(&fci, &fcc TSRMLS_CC) == FAILURE) {
			efree(params);
			zval_ptr_dtor(&retval_ptr);
			zend_error(E_WARNING, "Invocation of %s's constructor failed", PhurpleBuddyGroup_ce->name);
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


/* {{{ proto boolean PhurpleBuddyList::removeBuddy(PhurpleBuddy buddy)
	Removes a buddy from the buddy list */
PHP_METHOD(PhurpleBuddyList, removeBuddy)
{
	zval *buddy, *index;
	PurpleBuddy *pbuddy = NULL;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PhurpleBuddy_ce) == FAILURE) {
		RETURN_FALSE;
	}

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);

	index = zend_read_property(PhurpleBuddy_ce, buddy, "index", sizeof("index")-1, 0 TSRMLS_CC);
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


/* {{{ proto boolean PhurpleBuddyList::removeGroup(PhurpleBuddyGroup group)
	Removes an empty group from the buddy list */
PHP_METHOD(PhurpleBuddyList, removeGroup)
{
	zval *group, *index;
	PurpleGroup *pgroup = NULL;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PhurpleBuddyGroup_ce) == FAILURE) {
		RETURN_FALSE;
	}

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);

	index = zend_read_property(PhurpleBuddyGroup_ce, group, "index", sizeof("index")-1, 0 TSRMLS_CC);
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
** End phurple BuddyList methods
**
*/


/*
**
**
** Phurple BuddyGroup methods
**
*/

/* {{{ proto object PhurpleBuddyGroup::__construct(void)
	constructor*/
PHP_METHOD(PhurpleBuddyGroup, __construct)
{
	PurpleGroup *pgroup = NULL;
	char *name;
	int name_len;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	struct phurple_object_storage *pp = &PHURPLE_G(ppos);
	pgroup = purple_find_group(name);

	if(pgroup) {

		int ind = phurple_hash_index_find(&pp->group, pgroup);

		if(ind == FAILURE) {
			ulong nextid = zend_hash_next_free_element(&pp->group);
			zend_hash_index_update(&pp->group, nextid, pgroup, sizeof(PurpleGroup), NULL);
			zend_update_property_long(PhurpleBuddyGroup_ce,
			                          getThis(),
			                          "index",
			                          sizeof("index")-1,
			                          (long)nextid TSRMLS_CC
			                          );
		} else {
			zend_update_property_long(PhurpleBuddyGroup_ce,
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
		zend_update_property_long(PhurpleBuddyGroup_ce,
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


/* {{{ proto array PhurpleBuddyGroup::getAccounts(void)
	gets all the accounts related to the group */
PHP_METHOD(PhurpleBuddyGroup, getAccounts)
{
	PurpleGroup *pgroup = NULL;
	zval *index, *account;

	index = zend_read_property(PhurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PHURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);
	
	if(pgroup) {
		GSList *iter = purple_group_get_accounts(pgroup);
		
		if(iter && g_slist_length(iter)) {
			array_init(return_value);
			PHURPLE_MK_OBJ(account, PhurpleAccount_ce);
			
			for (; iter; iter = iter->next) {
				PurpleAccount *paccount = iter->data;
				
				if (paccount) {
					zend_update_property_long(PhurpleAccount_ce,
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


/* {{{ proto int PhurpleBuddyGroup::getSize(void)
	gets the count of the buddies in the group */
PHP_METHOD(PhurpleBuddyGroup, getSize)
{
	PurpleGroup *pgroup = NULL;
	zval *index;
	
	index = zend_read_property(PhurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PHURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
			RETURN_LONG(purple_blist_get_group_size(pgroup, (gboolean)TRUE));
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto int PhurpleBuddyGroup::getOnlineCount(void)
	gets the count of the buddies in the group with the status online*/
PHP_METHOD(PhurpleBuddyGroup, getOnlineCount)
{
	PurpleGroup *pgroup = NULL;
	zval *index;

	index = zend_read_property(PhurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PHURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

	if(pgroup) {
			RETURN_LONG(purple_blist_get_group_online_count(pgroup));
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PhurpleBuddyGroup::getName(void)
	gets the name of the group */
PHP_METHOD(PhurpleBuddyGroup, getName)
{
	PurpleGroup *pgroup = NULL;
	zval *index;

	index = zend_read_property(PhurpleBuddyGroup_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC);
	zend_hash_index_find(&PHURPLE_G(ppos).group, (ulong)Z_LVAL_P(index), (void**)&pgroup);

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
** End phurple BuddyGroup methods
**
*/

/*
**
** Helper functions
**
*/

/* {{{ */
static void
phurple_ui_init()
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
static zval
*phurple_string_zval(const char *str)
{
	zval *ret;
	
	MAKE_STD_ZVAL(ret);
	
	if ((char*)str) {
		ZVAL_STRING(ret, (char*)str, 1);
	} else {
		ZVAL_NULL(ret);
	}

	return ret;
}
/* }}} */


/* {{{ */
static zval
*phurple_long_zval(long l)
{
	zval *ret;
	MAKE_STD_ZVAL(ret);

	Z_TYPE_P(ret) = IS_LONG;
	Z_LVAL_P(ret) = l;

	return ret;
}
/* }}} */


/* {{{ */
static void
phurple_glib_io_destroy(gpointer data)
{
	g_free(data);
}
/* }}} */


/* {{{ */
static gboolean
phurple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleGLibIOClosure *closure = data;
	PurpleInputCondition phurple_cond = 0;
	
	if (condition & PHURPLE_GLIB_READ_COND)
		phurple_cond |= PURPLE_INPUT_READ;
	if (condition & PHURPLE_GLIB_WRITE_COND)
		phurple_cond |= PURPLE_INPUT_WRITE;
	
	closure->function(closure->data, g_io_channel_unix_get_fd(source),
	                  phurple_cond);
	
	return TRUE;
}
/* }}} */


/* {{{ */
static guint
glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
                               gpointer data)
{
	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;
	
	closure->function = function;
	closure->data = data;
	
	if (condition & PURPLE_INPUT_READ)
		cond |= PHURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PHURPLE_GLIB_WRITE_COND;
	
	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
	                                      phurple_glib_io_invoke, closure, phurple_glib_io_destroy);
	
	g_io_channel_unref(channel);
	return closure->result;
}
/* }}} */


/* {{{ */
static void
phurple_write_conv_function(PurpleConversation *conv, const char *who, const char *alias, const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const int PARAMS_COUNT = 5;
	zval ***params, *conversation, *buddy, *datetime, *retval, *tmp1, *tmp2, *tmp3;
	GList *conversations = purple_get_conversations();
	PurpleBuddy *pbuddy = NULL;
	PurpleAccount *paccount = NULL;

	TSRMLS_FETCH();

	PHURPLE_MK_OBJ(conversation, PhurpleConversation_ce);
	zend_update_property_long(PhurpleConversation_ce,
	                          conversation,
	                          "index",
	                          sizeof("index")-1,
	                          (long)g_list_position(conversations, g_list_find(conversations, conv)) TSRMLS_CC
	                          );

	zval *client = PHURPLE_G(phurple_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), g_list_position(purple_accounts_get_all(),g_list_find(purple_accounts_get_all(), (gconstpointer)purple_conversation_get_account(conv))));
	if(paccount) {
		pbuddy = purple_find_buddy(paccount, !who ? purple_conversation_get_name(conv) : who);
		
		if(pbuddy) {
			struct phurple_object_storage *pp = &PHURPLE_G(ppos);
			int ind = phurple_hash_index_find(&pp->buddy, pbuddy);
			PHURPLE_MK_OBJ(buddy, PhurpleBuddy_ce);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(PhurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PhurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)ind TSRMLS_CC
				                          );
			}
		} else {
			if(who) {
				buddy = phurple_string_zval(who);
			} else {
				ALLOC_INIT_ZVAL(buddy);
			}
		}
	}
	
	tmp1 = phurple_string_zval(message);
	tmp2 = phurple_long_zval((long)flags);
	tmp3 = phurple_long_zval((long)mtime);

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
phurple_write_im_function(PurpleConversation *conv, const char *who, const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const int PARAMS_COUNT = 5;
	zval ***params, *conversation, *buddy, *datetime, *retval, *tmp1, *tmp2, *tmp3;
	GList *conversations = purple_get_conversations();
	PurpleBuddy *pbuddy = NULL;
	PurpleAccount *paccount = NULL;

	TSRMLS_FETCH();

	PHURPLE_MK_OBJ(conversation, PhurpleConversation_ce);
	zend_update_property_long(PhurpleConversation_ce,
	                          conversation,
	                          "index",
	                          sizeof("index")-1,
	                          (long)g_list_position(conversations, g_list_find(conversations, conv)) TSRMLS_CC
	                          );

	zval *client = PHURPLE_G(phurple_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	paccount = g_list_nth_data (purple_accounts_get_all(), g_list_position(purple_accounts_get_all(),g_list_find(purple_accounts_get_all(), (gconstpointer)purple_conversation_get_account(conv))));
	if(paccount) {
		pbuddy = purple_find_buddy(paccount, !who ? purple_conversation_get_name(conv) : who);
		
		if(pbuddy) {
			struct phurple_object_storage *pp = &PHURPLE_G(ppos);
			int ind = phurple_hash_index_find(&pp->buddy, pbuddy);
			PHURPLE_MK_OBJ(buddy, PhurpleBuddy_ce);
			
			if(ind == FAILURE) {
				ulong nextid = zend_hash_next_free_element(&pp->buddy);
				zend_hash_index_update(&pp->buddy, nextid, pbuddy, sizeof(PurpleBuddy), NULL);
				zend_update_property_long(PhurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)nextid TSRMLS_CC
				                          );
			} else {
				zend_update_property_long(PhurpleBuddy_ce,
				                          buddy,
				                          "index",
				                          sizeof("index")-1,
				                          (long)ind TSRMLS_CC
				                         );
			}
		} else {
			if(who) {
				buddy = phurple_string_zval(who);
			} else {
				ALLOC_INIT_ZVAL(buddy);
			}
		}
	}
	
	tmp1 = phurple_string_zval(message);
	tmp2 = phurple_long_zval((long)flags);
	tmp3 = phurple_long_zval((long)mtime);

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
	zval_ptr_dtor(&conversation);

}
/* }}} */


/* {{{ */
static void
phurple_signed_on_function(PurpleConnection *conn, gpointer null)
{
	zval *connection, *retval;
	GList *connections = NULL;
	
	TSRMLS_FETCH();

	zval *client = PHURPLE_G(phurple_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	connections = purple_connections_get_all();

	PHURPLE_MK_OBJ(connection, PhurpleConnection_ce);
	zend_update_property_long(PhurpleConnection_ce,
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
#if PHURPLE_INTERNAL_DEBUG
	php_printf("==================== call_custom_method begin ============================\n");
	php_printf("class: %s\n", obj_ce->name);
	php_printf("method name: %s\n", function_name);
#endif
	int result, i;
	zend_fcall_info fci;
	zval z_fname, ***params, *retval;
	HashTable *function_table;
	va_list given_params;

	params = (zval ***) safe_emalloc(param_count, sizeof(zval **), 0);

	va_start(given_params, param_count);

#if PHURPLE_INTERNAL_DEBUG
	php_printf("param count: %d\n", param_count);
#endif
	for(i=0;i<param_count;i++) {
		params[i] = va_arg(given_params, zval **);
#if PHURPLE_INTERNAL_DEBUG
		php_printf("i=>%d: ", i);phurple_dump_zval(*params[i]);php_printf("\n");
#endif
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
#if PHURPLE_INTERNAL_DEBUG
	php_printf("==================== call_custom_method end ============================\n\n");
#endif
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
static char *phurple_tolower(const char *s)
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
static char *phurple_get_protocol_id_by_name(const char *protocol_name)
{
	GList *iter;

	iter = purple_plugins_get_protocols();

	for (; iter; iter = iter->next) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name && 0 == strcmp(phurple_tolower(info->name), phurple_tolower(protocol_name))) {
			return estrdup(info->id);
		}
	}

	return "";
}
/* }}} */


/* {{{ */
static int phurple_hash_index_find(HashTable *ht, void *element)
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
phurple_g_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
	/**
	 * @todo call here some php callback
	 */
}
/* }}} */


/* {{{ */
static void phurple_g_loop_callback(void)
{
	TSRMLS_FETCH();

	zval *client = PHURPLE_G(phurple_client_obj);
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
static int phurple_heartbeat_callback(gpointer data)
{
	TSRMLS_FETCH();

	zval *client = PHURPLE_G(phurple_client_obj);
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
phurple_request_authorize(PurpleAccount *account,
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

	zval *client = PHURPLE_G(phurple_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	zval *result, *php_account, *php_on_list, *php_remote_user, *php_message;
	
	if(NULL != account) {
		ALLOC_INIT_ZVAL(php_account);
		Z_TYPE_P(php_account) = IS_OBJECT;
		object_init_ex(php_account, PhurpleAccount_ce);
		zend_update_property_long(PhurpleAccount_ce,
		                          php_account,
		                          "index",
		                          sizeof("index")-1,
		                          (long)g_list_position(purple_accounts_get_all(), g_list_find(purple_accounts_get_all(), account)) TSRMLS_CC
		                          );
	} else {
		ALLOC_INIT_ZVAL(php_account);
	}
	
	MAKE_STD_ZVAL(php_on_list);
	ZVAL_BOOL(php_on_list, (long)on_list);
	
	php_message = phurple_string_zval(message);
	php_remote_user = phurple_string_zval(remote_user);
	
	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "authorizerequest",
	                   sizeof("authorizerequest")-1,
	                   &result,
	                   4,
	                   &php_account,
	                   &php_remote_user,
	                   &php_message,
	                   &php_on_list
	                   );
	
	if(Z_TYPE_P(result) == IS_BOOL || Z_TYPE_P(result) == IS_LONG || Z_TYPE_P(result) == IS_DOUBLE) {
		if((gboolean) Z_LVAL_P(result)) {
			auth_cb(user_data);
		} else {
			deny_cb(user_data);
		}
		
	}
}
/* }}} */


/* {{{ */
static void
phurple_signed_off_function(PurpleConnection *conn, gpointer null)
{
	zval *connection, *retval;
	GList *connections = NULL;

	TSRMLS_FETCH();

	zval *client = PHURPLE_G(phurple_client_obj);
	zend_class_entry *ce = Z_OBJCE_P(client);
	
	connections = purple_connections_get_all();

	PHURPLE_MK_OBJ(connection, PhurpleConnection_ce);
	zend_update_property_long(PhurpleConnection_ce,
	                          connection,
	                          "index",
	                          sizeof("index")-1,
	                          (long)g_list_position(connections, g_list_find(connections, conn)) TSRMLS_CC
	                          );

	call_custom_method(&client,
	                   ce,
	                   NULL,
	                   "onsignedoff",
	                   sizeof("onsignedoff")-1,
	                   NULL,
	                   1,
	                   &connection);
	
	zval_ptr_dtor(&connection);
}
/* }}} */

#if PHURPLE_INTERNAL_DEBUG
static void phurple_dump_zval(zval *var)
{

TSRMLS_FETCH();

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
#endif

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
