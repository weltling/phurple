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

#if PHURPLE_INTERNAL_DEBUG
void phurple_dump_zval(zval *var);
#endif

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
