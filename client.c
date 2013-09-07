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
extern char *segfault_message;

extern char *phurple_get_protocol_id_by_name(const char *name);
extern zval* call_custom_method(zval **object_pp, zend_class_entry *obj_ce, zend_function **fn_proxy, char *function_name, int function_name_len, zval **retval_ptr_ptr, int param_count, ... );

extern zval *
php_create_account_obj_zval(PurpleAccount *paccount TSRMLS_DC);

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

static int
phurple_heartbeat_callback(gpointer data)
{/* {{{ */
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

static void
phurple_signed_on_function(PurpleConnection *conn, gpointer null)
{/* {{{ */
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

static void
phurple_g_loop_callback(void)
{/* {{{ */
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

void
php_client_obj_destroy(void *obj TSRMLS_DC)
{
	struct ze_client_obj *zgo = (struct ze_client_obj *)obj;

	zend_object_std_dtor(&zgo->zo TSRMLS_CC);

	efree(zgo);
}

zend_object_value
php_client_obj_init(zend_class_entry *ce TSRMLS_DC)
{
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
}

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
	
	if(interval > 0) {
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

	account = purple_account_new(g_strdup(nick), phurple_get_protocol_id_by_name(protocol));

	if(NULL != account) {
		zval *ret;

		purple_account_set_password(account, estrdup(password));

		if(strlen(host)) {
			purple_account_set_string(account, "server", host);
		}

		if(strlen(port) && atoi(port)) {
			purple_account_set_int(account, "port", (int)atoi(port));
		}

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
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
	zval *account;
	PurpleAccount *paccount = NULL;
	GList *l;
	
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
	char *version = estrdup(purple_core_get_version());

	RETURN_STRING(version, 0);
}
/* }}} */


/* {{{ proto object PhurpleClient::getInstance(void)
	creates new PhurpleClient instance*/
PHP_METHOD(PhurpleClient, getInstance)
{	
	if(NULL == PHURPLE_G(phurple_client_obj)) {

		struct ze_client_obj *zco;
		zend_class_entry **ce = NULL;

		ALLOC_ZVAL(PHURPLE_G(phurple_client_obj));
		object_init_ex(PHURPLE_G(phurple_client_obj), EG(called_scope));
		INIT_PZVAL(PHURPLE_G(phurple_client_obj));
		
		zco = (struct ze_client_obj *) zend_object_store_get_object(PHURPLE_G(phurple_client_obj) TSRMLS_CC);

		/**
		 * phurple initialization stuff
		 */
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		zval **user_dir = zend_std_get_static_property(PhurpleClient_ce, "user_dir", sizeof("user_dir")-1, 0 TSRMLS_CC);
#else
		zval **user_dir = zend_std_get_static_property(PhurpleClient_ce, "user_dir", sizeof("user_dir")-1, 0, NULL TSRMLS_CC);
#endif
		purple_util_set_user_dir(g_strdup(Z_STRVAL_PP(user_dir)));
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		zval **debug = zend_std_get_static_property(PhurpleClient_ce, "debug", sizeof("debug")-1, 0 TSRMLS_CC);
#else
		zval **debug = zend_std_get_static_property(PhurpleClient_ce, "debug", sizeof("debug")-1, 0, NULL TSRMLS_CC);
#endif
		purple_debug_set_enabled(Z_LVAL_PP(debug));
		purple_core_set_ui_ops(&php_core_uiops);
		purple_accounts_set_ui_ops(&php_account_uiops);
		purple_eventloop_set_ui_ops(&glib_eventloops);
		/*purple_plugins_add_search_path(PHURPLE_G(custom_plugin_path));*/
		purple_plugins_add_search_path(INI_STR("phurple.custom_plugin_path"));

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
#endif
		if (!purple_core_init(g_strdup(Z_STRVAL_PP(ui_id)))) {
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
						  SIGNAL_SIGNED_ON,
						  &zco->connection_handle,
						  PURPLE_CALLBACK(phurple_signed_on_function),
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
