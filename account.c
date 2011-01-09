/**
 * Copyright (c) 2007-2008, Anatoliy Belsky
 *
 * This file is part of PHPhurple.
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

#include <php.h>

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
	Sets a protocol-specific setting for an account. The value types expected are int, string or bool. */
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
	Gets a protocol-specific setting for an account. Possible return datatypes are int|boolean|string or null if the setting isn't set or not found*/
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
