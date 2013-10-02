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
	zval *buddy, *group;
	struct ze_buddy_obj *zbo;
	struct ze_buddygroup_obj *zgo;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|O", &buddy, PhurpleBuddy_ce, &group, PhurpleBuddyGroup_ce) == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(buddy TSRMLS_CC);
	zgo = (struct ze_buddygroup_obj *) zend_object_store_get_object(group TSRMLS_CC);

	purple_blist_add_buddy(zbo->pbuddy, NULL, zgo->pbuddygroup, NULL);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool PhurpleBuddyList::addGroup(string name)
	Adds new group to the blist */
PHP_METHOD(PhurpleBuddyList, addGroup)
{
	zval *group;
	struct ze_buddygroup_obj *zgo;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PhurpleBuddyGroup_ce) == FAILURE) {
		RETURN_NULL();
	}

	zgo = (struct ze_buddygroup_obj *) zend_object_store_get_object(group TSRMLS_CC);
	
	purple_blist_add_group(zgo->pbuddygroup, NULL);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto PhurpleBuddy PhurpleBuddyList::findBuddy(PhurpleAccount account, string name)
	returns the buddy, if found */
PHP_METHOD(PhurpleBuddyList, findBuddy)
{
	zval *account;
	char *name;
	int name_len;
	PurpleBuddy *pbuddy;
	struct ze_account_obj *zao;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os", &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

	pbuddy = purple_find_buddy(zao->paccount, name);

	if(pbuddy) {
		zval *buddy;
		struct ze_buddy_obj *zbo;
		PHURPLE_MK_OBJ(buddy, PhurpleBuddy_ce);
		
		zbo = (struct ze_buddy_obj *) zend_object_store_get_object(buddy TSRMLS_CC);
		zbo->pbuddy = pbuddy;

		*return_value = *buddy;

		return;
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
	Finds group by name */
PHP_METHOD(PhurpleBuddyList, findGroup)
{
	PurpleGroup *pgroup = NULL;
	zval *name, *retval_ptr;
	zval ***params;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
		RETURN_NULL();
	}

	pgroup = purple_find_group(Z_STRVAL_P(name));

	if(pgroup) {
		params = safe_emalloc(sizeof(zval **), 1, 0);
		params[0] = &name;
		
		object_init_ex(return_value, PhurpleBuddyGroup_ce);
		
		fci.size = sizeof(fci);
		fci.function_table = EG(function_table);
		fci.function_name = NULL;
		fci.symbol_table = NULL;
		fci.retval_ptr_ptr = &retval_ptr;
		fci.param_count = 1;
		fci.params = params;
		fci.no_separation = 1;

		fcc.initialized = 1;
		fcc.function_handler = PhurpleBuddyGroup_ce->constructor;
		fcc.calling_scope = EG(scope);
		fcc.object_ptr = return_value;

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
	zval *buddy;
	struct ze_buddy_obj *zbo;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &buddy, PhurpleBuddy_ce) == FAILURE) {
		RETURN_FALSE;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(buddy TSRMLS_CC);

	purple_blist_remove_buddy(zbo->pbuddy);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto boolean PhurpleBuddyList::removeGroup(PhurpleBuddyGroup group)
	Removes an empty group from the buddy list */
PHP_METHOD(PhurpleBuddyList, removeGroup)
{
	zval *group;
	struct ze_buddygroup_obj *zgo;
	PurpleBlistNode *node;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PhurpleBuddyGroup_ce) == FAILURE) {
		RETURN_FALSE;
	}

	zgo = (struct ze_buddygroup_obj *) zend_object_store_get_object(group TSRMLS_CC);

	node = (PurpleBlistNode *) zgo->pbuddygroup;

	if(node->child) {
		/* group isn't empty */
		RETURN_FALSE;
	}
	
	purple_blist_remove_group(zgo->pbuddygroup);

	RETURN_TRUE;
}
/* }}} */

/*
**
**
** End phurple BuddyList methods
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
