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
#include "Zend/zend_exceptions.h"

#include "php_phurple.h"

#include <glib.h>

#include <string.h>
#include <ctype.h>

#include <purple.h>

extern char *phurple_get_protocol_id_by_name(const char *name);

/**
 * Took this from the libphurples account.c because of need
 * to get the account settings. If the libphurple will change,
 * should fit it as well.
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

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

void
php_account_obj_destroy(void *obj TSRMLS_DC)
{
	struct ze_account_obj *zao = (struct ze_account_obj *)obj;

	zend_object_std_dtor(&zao->zo TSRMLS_CC);

	/* IMPORTANT! don't destroy an account when object is destructing,
	 	every account object is persistent within libpurple once created.
		The same for buddy, group, etc. */
	/*if (zao->paccount) {
		purple_account_destroy(zao->paccount);
	}*/

	efree(zao);
}

zend_object_value
php_account_obj_init(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ret;
	struct ze_account_obj *zao;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zval *tmp;
#endif

	zao = (struct ze_account_obj *) emalloc(sizeof(struct ze_account_obj));
	memset(&zao->zo, 0, sizeof(zend_object));

	zend_object_std_init(&zao->zo, ce TSRMLS_CC);
#if PHP_MAJOR_VERSION== 5 && PHP_MINOR_VERSION < 4
	zend_hash_copy(zao->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref,
					(void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&zao->zo, ce);
#endif

	zao->paccount = NULL;

	ret.handle = zend_objects_store_put(zao, NULL,
								(zend_objects_free_object_storage_t) php_account_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}

zval *
php_create_account_obj_zval(PurpleAccount *paccount TSRMLS_DC)
{
	zval *ret;
	struct ze_account_obj *zao;

	ALLOC_ZVAL(ret);
	object_init_ex(ret, PhurpleAccount_ce);
	INIT_PZVAL(ret);

	zao = (struct ze_account_obj *) zend_object_store_get_object(ret TSRMLS_CC);
	zao->paccount = paccount;

	return ret;
}

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
	char *username, *protocol_name;
	int username_len, protocol_name_len;
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &username, &username_len, &protocol_name, &protocol_name_len) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	zao->paccount = purple_account_new(username, phurple_get_protocol_id_by_name(protocol_name));

	if (NULL == zao->paccount) {
		zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Failed to create account");
		return;
	}

	purple_accounts_add(zao->paccount);
}
/* }}} */


/* {{{ proto void PhurpleAccount::setPassword(int account, string password)
	Sets the account's password */
PHP_METHOD(PhurpleAccount, setPassword)
{
	int password_len;
	char *password;
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	purple_account_set_password(zao->paccount, password);
}
/* }}} */


/* {{{ proto void PhurpleAccount::setEnabled(bool enabled)
	Sets whether or not this account is enabled for some UI */
PHP_METHOD(PhurpleAccount, setEnabled)
{
	zend_bool enabled;
	struct ze_account_obj *zao;
	zval **ui_id = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &enabled) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
#endif
	purple_account_set_enabled(zao->paccount, Z_STRVAL_PP(ui_id), (gboolean) enabled);
}
/* }}} */


/* {{{ proto bool PhurpleAccount::addBuddy(PhurpleBuddy buddy)
	Adds a buddy to the server-side buddy list for the specified account */
PHP_METHOD(PhurpleAccount, addBuddy)
{
	zval *buddy;
	struct ze_account_obj *zao;
	struct ze_buddy_obj *zbo;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PhurpleBuddy_ce) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(buddy TSRMLS_CC);

	purple_blist_add_buddy(zbo->pbuddy, NULL, NULL, NULL);
	purple_account_add_buddy(zao->paccount, zbo->pbuddy);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool PhurpleAccount::removeBuddy(PhurpleBuddy buddy)
	Removes a buddy from the server-side buddy list for the specified account */
PHP_METHOD(PhurpleAccount, removeBuddy)
{
	zval *buddy;
	struct ze_account_obj *zao;
	struct ze_buddy_obj *zbo;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PhurpleBuddy_ce) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(buddy TSRMLS_CC);

	purple_account_remove_buddy(zao->paccount, zbo->pbuddy, purple_buddy_get_group(zbo->pbuddy));

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto void PhurpleAccount::clearSettings(void)
	Clears all protocol-specific settings on an account. */
PHP_METHOD(PhurpleAccount, clearSettings)
{
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	purple_account_clear_settings(zao->paccount);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto void PhurpleAccount::set(string name, string value)
	Sets a protocol-specific setting for an account. The value types expected are int, string or bool. */
PHP_METHOD(PhurpleAccount, set)
{
	zval *value;
	char *name;
	int name_len;
	struct ze_account_obj *zao;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
#endif
	
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &name_len, &value) == FAILURE) {
		RETURN_FALSE;
	}
	
	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	switch (Z_TYPE_P(value)) {
		case IS_BOOL:
			purple_account_set_ui_bool (zao->paccount, Z_STRVAL_PP(ui_id), name, (gboolean) Z_LVAL_P(value));
		break;
		
		case IS_LONG:
		case IS_DOUBLE:
			purple_account_set_ui_int (zao->paccount, Z_STRVAL_PP(ui_id), name, (int) Z_LVAL_P(value));
		break;
			
		case IS_STRING:
			purple_account_set_ui_string (zao->paccount, Z_STRVAL_PP(ui_id), name, Z_STRVAL_P(value));
		break;
			
		default:
			RETURN_FALSE;
		break;
	}

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto mixed PhurpleAccount::get($key)
	Gets a protocol-specific setting for an account. Possible return datatypes are int|boolean|string or null if the setting isn't set or not found*/
PHP_METHOD(PhurpleAccount, get)
{
	PurpleAccountSetting *setting;
	GHashTable *table = NULL;
	char *name;
	int name_len;
	struct ze_account_obj *zao;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0 TSRMLS_CC);
#else
		zval **ui_id = zend_std_get_static_property(PhurpleClient_ce, "ui_id", sizeof("ui_id")-1, 0, NULL TSRMLS_CC);
#endif
	
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if((table = g_hash_table_lookup(zao->paccount->ui_settings, Z_STRVAL_PP(ui_id))) == NULL) {
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
		}
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto boolean PhurpleAccount::isConnected(void)
	Returns whether or not the account is connected*/
PHP_METHOD(PhurpleAccount, isConnected)
{
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}
	
	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETVAL_BOOL((long) purple_account_is_connected(zao->paccount));
}
/* }}} */


/* {{{ proto boolean PhurpleAccount::isConnecting(void)
	Returns whether or not the account is connecting*/
PHP_METHOD(PhurpleAccount, isConnecting)
{
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}
	
	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	RETVAL_BOOL((long) purple_account_is_connecting(zao->paccount));
}
/* }}} */


/* {{{ proto string PhurpleAccount::getUserName(void) Returns the account's username */
PHP_METHOD(PhurpleAccount, getUserName)
{
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}
	
	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	RETURN_STRING(purple_account_get_username(zao->paccount), 1);
}
/* }}} */


/* {{{ proto string PhurpleAccount::getPassword(void) Returns the account's password */
PHP_METHOD(PhurpleAccount, getPassword)
{
	struct ze_account_obj *zao;
	
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}
	
	zao = (struct ze_account_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	RETURN_STRING(purple_account_get_password(zao->paccount), 1);
}
/* }}} */

/*
**
**
** End phurple account methods
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
