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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
