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

#include "php_phurple.h"

#include <glib.h>

#include <string.h>
#include <ctype.h>

#include <purple.h>

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

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
		/*const char *name = purple_buddy_get_name(pbuddy);
		if(name && '\0' != name) {
			RETURN_STRING(estrdup(name), 0);
		}*/
		RETURN_STRING((char*)purple_buddy_get_name(pbuddy), 0);
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
