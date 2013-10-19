/**
 * Copyright (c) 2007-2011, Anatol Belski <ab@php.net>
 *
 * This file is part of Phurple.
 *
 * Phurple is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Phurple is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Phurple.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include "Zend/zend_exceptions.h"

#ifdef HAVE_BUNDLED_PCRE
#include <ext/pcre/pcrelib/pcre.h>
#elif HAVE_PCRE
#include <pcre.h>
#endif

#include "php_phurple.h"

#include <glib.h>

#include <string.h>
#include <ctype.h>

#include <purple.h>


extern PurpleEventLoopUiOps glib_eventloops;
extern PurpleCoreUiOps php_core_uiops;
extern PurpleAccountUiOps php_account_uiops;
extern PurpleRequestUiOps php_request_uiops;
#if defined(HAVE_SIGNAL_H) && !defined(PHP_WIN32)
extern char *segfault_message;
#endif

extern char *phurple_get_protocol_id_by_name(const char *name);
extern zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... );

extern zval *
php_create_account_obj_zval(PurpleAccount *paccount TSRMLS_DC);

extern zval *
php_create_connection_obj_zval(PurpleConnection *pconnection TSRMLS_DC);

extern zval *
php_create_conversation_obj_zval(PurpleConversation *pconv TSRMLS_DC);

extern zval*
phurple_long_zval(long l);

extern zval*
phurple_string_zval(const char *s);

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

static int
phurple_heartbeat_callback(gpointer data)
{/* {{{ */
	zval *client;
	zend_class_entry *ce;
	TSRMLS_FETCH();

	client = PHURPLE_G(phurple_client_obj);
	ce = Z_OBJCE_P(client);

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

static void
phurple_signed_all_cb(char *php_method, PurpleConnection *conn)
{/*{{{*/
	zval *connection;
	zval *client;
	zend_class_entry *ce;
	TSRMLS_FETCH();

	if (!conn) {
		ALLOC_INIT_ZVAL(connection);
		ZVAL_NULL(connection);
	} else {
		connection = php_create_connection_obj_zval(conn TSRMLS_CC);
	}

	client = PHURPLE_G(phurple_client_obj);
	ce = Z_OBJCE_P(client);
	
	call_custom_method(&client,
					   ce,
					   NULL,
					   php_method,
					   strlen(php_method),
					   NULL,
					   1,
					   &connection);
	
	zval_ptr_dtor(&connection);
}/*}}}*/

static void
phurple_signed_on_function(PurpleConnection *conn)
{/* {{{ */
	phurple_signed_all_cb("onsignedon", conn);
}/* }}} */

static void
phurple_signing_on_function(PurpleConnection *conn)
{/* {{{ */
	phurple_signed_all_cb("onsigningon", conn);
}/* }}} */

static void
phurple_signed_off_function(PurpleConnection *conn)
{/* {{{ */
	phurple_signed_all_cb("onsignedoff", conn);
}/* }}} */

static void
phurple_signing_off_function(PurpleConnection *conn)
{/* {{{ */
	phurple_signed_all_cb("onsigningoff", conn);
}/* }}} */

static void
phurple_connection_error_function(PurpleConnection *conn, PurpleConnectionError err, const gchar *desc)
{/* {{{ */
	zval *connection;
	zval *client;
	zval *tmp1, *tmp2;
	zend_class_entry *ce;
	TSRMLS_FETCH();

	if (!conn) {
		ALLOC_INIT_ZVAL(connection);
		ZVAL_NULL(connection);
	} else {
		connection = php_create_connection_obj_zval(conn TSRMLS_CC);
	}
	tmp1 = phurple_long_zval((long)err);
	tmp2 = phurple_string_zval(desc);

	client = PHURPLE_G(phurple_client_obj);
	ce = Z_OBJCE_P(client);
	
	call_custom_method(&client,
					   ce,
					   NULL,
					   "onconnectionerror",
					   sizeof("onconnectionerror")-1,
					   NULL,
					   3,
					   &connection,
					   &tmp1,
					   &tmp2
	);
	
	zval_ptr_dtor(&connection);
	zval_ptr_dtor(&tmp1);
	zval_ptr_dtor(&tmp2);
}
/* }}} */

static gboolean
phurple_autojoin_function(PurpleConnection *conn)
{/* {{{ */
	zval *connection;
	zval *client;
	zval *method_ret = NULL;
	zend_class_entry *ce;
	TSRMLS_FETCH();

	if (!conn) {
		ALLOC_INIT_ZVAL(connection);
		ZVAL_NULL(connection);
	} else {
		connection = php_create_connection_obj_zval(conn TSRMLS_CC);
	}

	client = PHURPLE_G(phurple_client_obj);
	ce = Z_OBJCE_P(client);
	
	call_custom_method(&client,
					   ce,
					   NULL,
					   "onautojoin",
					   sizeof("onautojoin")-1,
					   &method_ret,
					   1,
					   &connection
	);
	
	zval_ptr_dtor(&connection);

	convert_to_boolean(method_ret);

	return Z_BVAL_P(method_ret);
}
/* }}} */

static gboolean
phurple_writing_im_msg(PurpleAccount *account, const char *who, char **message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_wrote_im_msg(PurpleAccount *account, const char *who, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_sending_im_msg(PurpleAccount *account, const char *receiver, char **message)
{/*{{{*/

}/*}}}*/

static void
phurple_sent_im_msg(PurpleAccount *account, const char *receiver, const char *message)
{/*{{{*/

}/*}}}*/

static gboolean
phurple_receiving_im_msg(PurpleAccount *account, char **sender, char **message, PurpleConversation *conv, PurpleMessageFlags *flags)
{/*{{{*/

}/*}}}*/

static void
phurple_received_im_msg(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static gboolean 
phurple_writing_chat_msg(PurpleAccount *account, const char *who, char **message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_wrote_chat_msg(PurpleAccount *account, const char *who, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_sending_chat_msg(PurpleAccount *account, char **message, int id)
{/*{{{*/

}/*}}}*/

static void
phurple_sent_chat_msg(PurpleAccount *account, const char *message, int id)
{/*{{{*/

}/*}}}*/

static gboolean
phurple_receiving_chat_msg(PurpleAccount *account, char **sender, char **message, PurpleConversation *conv, int *flags)
{/*{{{*/

}/*}}}*/

static void
phurple_blocked_im_msg(PurpleAccount *account, const char *sender, const char *message, PurpleMessageFlags flags, time_t when)
{/*{{{*/

}/*}}}*/

static void
phurple_received_chat_msg(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_conversation_created(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_conversation_updated(PurpleConversation *conv, PurpleConvUpdateType type)
{/*{{{*/

}/*}}}*/

static void
phurple_deleting_conversation(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_buddy_typing(PurpleAccount *account, const char *name)
{/*{{{*/

}/*}}}*/

static void
phurple_buddy_typing_stopped(PurpleAccount *account, const char *name)
{/*{{{*/

}/*}}}*/

static gboolean
phurple_chat_buddy_joining(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_buddy_joined(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags flags, gboolean new_arrival)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_join_failed(PurpleConnection *gc, GHashTable *components)
{/*{{{*/

}/*}}}*/

static gboolean
phurple_chat_buddy_leaving(PurpleConversation *conv, const char *name, const char *reason)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_buddy_left(PurpleConversation *conv, const char *name, const char *reason)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_inviting_user(PurpleConversation *conv, const char *name, char **invite_message)
{/*{{{*/

}/*}}}*/

static void 
phurple_chat_invited_user(PurpleConversation *conv, const char *name, const char *invite_message)
{/*{{{*/

}/*}}}*/

static gint
phurple_chat_invited(PurpleAccount *account, const char *inviter, const char *chat, const char *invite_message, const GHashTable *components)
{/*{{{*/

}/*}}}*/

static void 
phurple_chat_invite_blocked(PurpleAccount *account, const char *inviter, const char *name, const char *message, GHashTable *data)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_joined(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_left(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_topic_changed(PurpleConversation *conv, const char *who, const char *topic)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_buddy_flags(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags oldflags, PurpleConvChatBuddyFlags newflags)
{/*{{{*/

}/*}}}*/

static void
phurple_g_loop_callback(gpointer data)
{/* {{{ */
	zval *client;
	zend_class_entry *ce;
	TSRMLS_FETCH();

	client = PHURPLE_G(phurple_client_obj);
	ce = Z_OBJCE_P(client);

	call_custom_method(&client,
					   ce,
					   NULL,
					   "loopcallback",
					   sizeof("loopcallback")-1,
					   NULL,
					   0);
}
/* }}} */

void
php_client_obj_destroy(void *obj TSRMLS_DC)
{/*{{{*/
	struct ze_client_obj *zgo = (struct ze_client_obj *)obj;

	zend_object_std_dtor(&zgo->zo TSRMLS_CC);

	efree(zgo);
}/*}}}*/

zend_object_value
php_client_obj_init(zend_class_entry *ce TSRMLS_DC)
{/*{{{*/
	zend_object_value ret;
	struct ze_client_obj *zco;
#if PHP_MAJOR_VERSION > 5 || PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zval *tmp;
#endif

	zco = (struct ze_client_obj *) emalloc(sizeof(struct ze_client_obj));
	memset(&zco->zo, 0, sizeof(zend_object));


	zend_object_std_init(&zco->zo, ce TSRMLS_CC);

#if PHP_MAJOR_VERSION > 5 || PHP_MAJOR_VERSION== 5 && PHP_MINOR_VERSION < 4
	zend_hash_copy(zco->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref,
					(void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&zco->zo, ce);
#endif

	zco->connection_handle = 0;

	ret.handle = zend_objects_store_put(zco, NULL,
								(zend_objects_free_object_storage_t) php_client_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}/*}}}*/

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
	GMainLoop *loop;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &interval) == FAILURE) {
		RETURN_NULL();
	}
	
	phurple_g_loop_callback(NULL);

	if(interval > 0) {
		g_timeout_add(interval, (GSourceFunc)phurple_heartbeat_callback, NULL);
	}
	
	loop = g_main_loop_new(NULL, FALSE);
	
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
		zend_throw_exception_ex(NULL, 0 TSRMLS_CC, "PCRE compilation failed at offset %d: %s", erroffset, error);
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
				zend_throw_exception_ex(NULL, 0 TSRMLS_CC, "The account string must match \"protocol://user:password@host:port pattern\". Matching error %d", rc);
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

	account = purple_account_new(g_strdup(nick), phurple_get_protocol_id_by_name(protocol));

	if(NULL != account) {
		zval *ret;
		zval **ui_id = NULL;

		purple_account_set_password(account, estrdup(password));

		if(strlen(host)) {
			purple_account_set_string(account, "server", host);
		}

		if(strlen(port) && atoi(port)) {
			purple_account_set_int(account, "port", (int)atoi(port));
		}

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
#endif
		purple_account_set_enabled(account, g_strdup(Z_STRVAL_PP(ui_id)), 1);

		purple_accounts_add(account);

		accounts = purple_accounts_get_all();

		ret = php_create_account_obj_zval(account TSRMLS_CC);
		*return_value = *ret;

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
			if (PhurpleAccount_ce == Z_OBJCE_P(account)) {
				struct ze_account_obj *zao;

				zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

				paccount = zao->paccount;
			}
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
	PurpleAccount *paccount = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &account_name, &account_name_len) == FAILURE) {
		RETURN_NULL();
	}

	paccount = purple_accounts_find(account_name, NULL);

	if(paccount) {
		zval *ret = php_create_account_obj_zval(paccount TSRMLS_CC);

		*return_value = *ret;

		return;
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PhurpleClient::getCoreVersion(void)
	Returns the libphurple core version string */
PHP_METHOD(PhurpleClient, getCoreVersion)
{	
	const char *version = purple_core_get_version();

	RETURN_STRING(version, 1);
}
/* }}} */


/* {{{ proto object PhurpleClient::getInstance(void)
	creates new PhurpleClient instance*/
PHP_METHOD(PhurpleClient, getInstance)
{	
	if(NULL == PHURPLE_G(phurple_client_obj)) {

		struct ze_client_obj *zco;
		zval **user_dir = NULL, **debug = NULL, **ui_id = NULL;
		PurpleSavedStatus *saved_status;

		ALLOC_ZVAL(PHURPLE_G(phurple_client_obj));
		object_init_ex(PHURPLE_G(phurple_client_obj), EG(called_scope));
		INIT_PZVAL(PHURPLE_G(phurple_client_obj));
		
		zco = (struct ze_client_obj *) zend_object_store_get_object(PHURPLE_G(phurple_client_obj) TSRMLS_CC);

		/**
		 * phurple initialization stuff
		 */
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		user_dir = zend_std_get_static_property(PhurpleClient_ce, "user_dir", sizeof("user_dir")-1, 0 TSRMLS_CC);
#else
		user_dir = zend_std_get_static_property(PhurpleClient_ce, "user_dir", sizeof("user_dir")-1, 0, NULL TSRMLS_CC);
#endif
		purple_util_set_user_dir(g_strdup(Z_STRVAL_PP(user_dir)));
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		debug = zend_std_get_static_property(PhurpleClient_ce, "debug", sizeof("debug")-1, 0 TSRMLS_CC);
#else
		debug = zend_std_get_static_property(PhurpleClient_ce, "debug", sizeof("debug")-1, 0, NULL TSRMLS_CC);
#endif
		purple_debug_set_enabled(Z_LVAL_PP(debug));
		purple_core_set_ui_ops(&php_core_uiops);
		purple_accounts_set_ui_ops(&php_account_uiops);
		purple_request_set_ui_ops(&php_request_uiops);
		purple_eventloop_set_ui_ops(&glib_eventloops);
		/*purple_plugins_add_search_path(PHURPLE_G(custom_plugin_path));*/
		purple_plugins_add_search_path(INI_STR("phurple.custom_plugin_path"));

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
#endif
		if (!purple_core_init(Z_STRVAL_PP(ui_id))) {
#if defined(HAVE_SIGNAL_H) && !defined(PHP_WIN32)
			g_free(segfault_message);
#endif
			zend_throw_exception(NULL, "Couldn't initalize the libphurple core", 0 TSRMLS_CC);
			RETURN_NULL();
		}
	
		purple_set_blist(purple_blist_new());
		purple_blist_load();
		
		purple_prefs_load();

		saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
		purple_savedstatus_activate(saved_status);

		*return_value = *PHURPLE_G(phurple_client_obj);

		call_custom_method(&PHURPLE_G(phurple_client_obj),
						   Z_OBJCE_P(PHURPLE_G(phurple_client_obj)),
						   NULL,
						   "initinternal",
						   sizeof("initinternal")-1,
						   NULL,
						  0);

		return;
	} else {
		Z_ADDREF_P(PHURPLE_G(phurple_client_obj));
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


/* {{{ proto void PhurpleClient::setUserDir(string $user_dir)
	Define a custom phurple settings directory, overriding the default (user's home directory/.phurple) */
PHP_METHOD(PhurpleClient, setUserDir) {
	char *user_dir;
	int user_dir_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user_dir, &user_dir_len) == FAILURE) {
		return;
	}

	/*if (PHURPLE_G(custom_user_dir)) {
		efree(PHURPLE_G(custom_user_dir));
	}*/

	zend_update_static_property_string(PhurpleClient_ce, "user_dir", strlen("user_dir"), user_dir TSRMLS_CC);
	
	purple_util_set_user_dir(g_strdup(user_dir));
}
/* }}} */


/* {{{ proto void PhurpleClient::setDebug(boolean $debug)
	Turn debug on or off, default off*/
PHP_METHOD(PhurpleClient, setDebug)
{
	zval *debug;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &debug) == FAILURE) {
		return;
	}

	convert_to_long(debug);

	zend_update_static_property_long(PhurpleClient_ce, "debug", sizeof("debug")-1, Z_LVAL_P(debug) TSRMLS_CC);

	purple_debug_set_enabled(Z_LVAL_P(debug));
}
/* }}} */


/* {{{ proto void PhurpleClient::setUiId(string $ui_id)
	Set ui id*/
PHP_METHOD(PhurpleClient, setUiId)
{
	char *ui_id;
	int ui_id_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ui_id, &ui_id_len) == FAILURE) {
		return;
	}

	zend_update_static_property_string(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, ui_id TSRMLS_CC);
}
/* }}} */


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
	struct ze_client_obj *zco;

	zco = (struct ze_client_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	purple_signal_connect(purple_connections_get_handle(),
						  "signed-on",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_signed_on_function),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "signing-on",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_signing_on_function),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "signed-off",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_signed_off_function),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "signing-off",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_signing_off_function),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "connection-error",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_connection_error_function),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "autojoin",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_autojoin_function),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "writing-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_writing_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "wrote-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_wrote_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "sending-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_sending_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "sent-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_sent_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "receiving-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_receiving_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "received-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_received_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "blocked-im-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_blocked_im_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "writing-chat-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_writing_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "wrote-chat-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_wrote_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "sending-chat-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_sending_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "sent-chat-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_sent_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "receiving-chat-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_receiving_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "received-chat-msg",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_received_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "conversation-created",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_conversation_created),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "conversation-updated",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_conversation_updated),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "deleting-conversation",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_deleting_conversation),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "buddy-typing",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_buddy_typing),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "buddy-typing-stopped",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_buddy_typing_stopped),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-buddy-joined",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_buddy_joined),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-buddy-joined",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_buddy_joined),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-buddy-leaving",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_buddy_leaving),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-buddy-left",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_buddy_left),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-inviting-user",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_inviting_user),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-invited-user",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_invited_user),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-invited",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_invited),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-invite-blocked",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_invite_blocked),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-joined",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_joined),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-join-failed",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_join_failed),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-left",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_left),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-topic-changed",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_topic_changed),
						  NULL
	);

	purple_signal_connect(purple_connections_get_handle(),
						  "chat-buddy-flags",
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_chat_buddy_flags),
						  NULL
	);
}
/* }}} */


/* {{{ proto void PhurpleClient::disconnect()
	Close all client connections*/
/*PHP_METHOD(PhurpleClient, disconnect)
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
}*/
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

/* {{{ proto void Phurple\Client::onConnectionError(Phurple\Connection connection)
	This callback is called when connection error occurs, if implemented */
PHP_METHOD(PhurpleClient, onConnectionError)
{
}
/* }}} */

/* {{{ proto void Phurple\Client::onSigningOn(Phurple\Connection connection)
	This callback is called when the client is about to sign on, if implemented */
PHP_METHOD(PhurpleClient, onSigningdOn)
{

}
/* }}} */

/* {{{ proto void Phurple\Client::onSigningOff(Phurple\Connection connection)
	This callback is called when the client is about to sign off, if implemented */
PHP_METHOD(PhurpleClient, onSigningOff)
{

}
/* }}} */

/* {{{ proto boolean Phurple\Client::onAutojoin(Phurple\Connection connection)
	This callback is called when the connection has signed on, if implemented */
PHP_METHOD(PhurpleClient, onAutojoin)
{
	RETURN_FALSE;
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


/* {{{ void Phurple\Client::onSignedOff($connection) signed off callback */
PHP_METHOD(PhurpleClient, onSignedOff)
{

}
/* }}} */


/* {{{ proto public int Phurple\Client::requestAction(string title, string primary, string secondary, integer default_action, Phurple\Account account, string who, Phurple\Conversation conv, array actions)
 	Handle action requests received from server or elsewhere.*/
PHP_METHOD(PhurpleClient, requestAction)
{

}
/* }}} */

/* {{{ protected boolean Phurple\Client::writinImMsg(string who, string &message, Phurple\Conversation conv) 
	This callback is invoked before the message is written into conversation. Return true to cancel the msg. */
PHP_METHOD(PhurpleClient, writingImMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::wroteImMsg(string who, string message, Phurple\Conversation conv) 
	This callback is invoked after the message is written. */
PHP_METHOD(PhurpleClient, wroteImMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::sendingImMsg(string receiver, string &message) 
	This callback is invoked before sending IM. */
PHP_METHOD(PhurpleClient, sendingImMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::sentImMsg(string receiver, string &message) 
	This callback is invoked after IM is sent. */
PHP_METHOD(PhurpleClient, sentImMsg)
{

}
/* }}} */

/* {{{ protected boolean Phurple\Client::receivingImMsg(string &sender, string &message, Phurple\Conversation conv) 
	This callback is invoked when the message is received. Return true to cancel the msg. */
PHP_METHOD(PhurpleClient, receivingImMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::receivedImMsg(string sender, string message, Phurple\Conversation conv) 
	This callback is invoked after the IM is received. */
PHP_METHOD(PhurpleClient, receivedImMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::blockedImMsg(string sender, string message, integer timestamp) 
	This callback is invoked after the IM is blocked due to privacy settings. */
PHP_METHOD(PhurpleClient, blockedImMsg)
{

}
/* }}} */

/* {{{ protected boolean Phurple\Client::writinChatMsg(string who, string &message, Phurple\Conversation conv) 
	This callback is invoked before the message is written into chat conversation. Return true to cancel the msg. */
PHP_METHOD(PhurpleClient, writingChatMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::wroteChatMsg(string who, string message, Phurple\Conversation conv) 
	This callback is invoked after the message is written to chat conversation. */
PHP_METHOD(PhurpleClient, wroteChatMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::sendingChatMsg(string &message, integer chatId) 
	This callback is invoked before sending message to a chat. */
PHP_METHOD(PhurpleClient, sendingChatMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::sentImMsg(string receiver, string &message) 
	This callback is invoked after IM is sent. */
PHP_METHOD(PhurpleClient, sentChatMsg)
{

}
/* }}} */

/* {{{ protected boolean Phurple\Client::receivingChatMsg(string &sender, string &message, Phurple\Conversation conv) 
	This callback is invoked when chat message is received. Return true to cancel the msg. */
PHP_METHOD(PhurpleClient, receivingChatMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::receivedChatMsg(string sender, string message, Phurple\Conversation conv) 
	This callback is invoked after chat message is received. */
PHP_METHOD(PhurpleClient, receivedChatMsg)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::conversationCreated(Phurple\Conversation conv) 
	This callback is invoked when a new conversation is created. */
PHP_METHOD(PhurpleClient, conversationCreated)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::conversationUpdated(Phurple\Conversation conv, integer type) 
	This callback is invoked when a new conversation is updated. */
PHP_METHOD(PhurpleClient, conversationUpdated)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::deletingConversation(Phurple\Conversation conv) 
	This callback is invoked when a conversation is about destroyed. */
PHP_METHOD(PhurpleClient, deletingConversation)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::buddyTyping(Phurple\Account account, string name) 
	This callback is invoked when buddy starts typing into conversation. */
PHP_METHOD(PhurpleClient, buddyTyping)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::buddyTypingStopped(Phurple\Account account, string name) 
	This callback is invoked when buddy stops typing into conversation. */
PHP_METHOD(PhurpleClient, buddyTypingStopped)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatBuddyJoining(Phurple\Conversation conv, string name, integer buddyflags) 
	This callback is invoked when buddy is joining a chat. */
PHP_METHOD(PhurpleClient, chatBuddyJoining)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatBuddyJoined(Phurple\Conversation conv, string name, boolean new_arrival, integer buddyflags) 
	This callback is invoked when buddy has joined a chat. */
PHP_METHOD(PhurpleClient, chatBuddyJoined)
{

}
/* }}} */

/* {{{ protected boolean Phurple\Client::chatBuddyLeaving(Phurple\Conversation conv, string name, string reason) 
	This callback is invoked when user is leaving a chat. Return true to hide the leave. */
PHP_METHOD(PhurpleClient, chatBuddyLeaving)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatBuddyLeft(Phurple\Conversation conv, string name, string reason) 
	This callback is invoked when user left a chat. */
PHP_METHOD(PhurpleClient, chatBuddyLeft)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatInvitingUser(Phurple\Conversation conv, string name, string &invite_message) 
	This callback is invoked when user is being invited to chat. The callback can replace the invite message. */
PHP_METHOD(PhurpleClient, chatInvitingUser)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatInvitedUser(Phurple\Conversation conv, string name, string invite_message) 
	This callback is invoked when user invited another user to chat. */
PHP_METHOD(PhurpleClient, chatInvitedUser)
{

}
/* }}} */

/* {{{ protected integer Phurple\Client::chatInvited(string inviter, string chat, string invite_message) 
	This callback is invoked when account was invited to a chat. Return -1 to reject, 1 to accept, 0 for user to be prompted.*/
PHP_METHOD(PhurpleClient, chatInvited)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatInviteBlocked(string inviter, string chat, string message) 
	This callback is invoked when invitation to join a chat was blocked. */
PHP_METHOD(PhurpleClient, chatInviteBlocked)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatJoined(Phurple\Conversation conv) 
	This callback is invoked when account joined a chat. */
PHP_METHOD(PhurpleClient, chatJoined)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatJoinFailed(Phurple\Conversation conv) 
	This callback is invoked when account failed to join a chat. */
PHP_METHOD(PhurpleClient, chatJoinFalied)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatLeft(Phurple\Conversation conv) 
	This callback is invoked when account left a chat. */
PHP_METHOD(PhurpleClient, chatLeft)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatTopicChanged(Phurple\Conversation conv, string who, string topic) 
	This callback is invoked when the chat topic is changed. */
PHP_METHOD(PhurpleClient, chatTopicChanged)
{

}
/* }}} */

/* {{{ protected void Phurple\Client::chatTopicChanged(Phurple\Conversation conv, string name, integer oldflags, integer newflags) 
	This callback is invoked when flags of a user in chat are changed. */
PHP_METHOD(PhurpleClient, chatBuddyFlags)
{

}
/* }}} */

/*
**
**
** End phurple client callback methods
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
