/**
 * Copyright (c) 2007-2014, Anatol Belski <ab@php.net>
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

extern zval *
php_create_buddy_obj_zval(PurpleBuddy *pbuddy TSRMLS_DC);

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

/*zval *
php_create_buddylist_obj_zval(PurpleBuddyList *pbuddylist TSRMLS_DC)
{
	zval *ret;
	struct ze_buddylist_obj *zao;

	if (!pbuddy) {
		ALLOC_INIT_ZVAL(ret);
		ZVAL_NULL(ret);
	} else {
		ALLOC_ZVAL(ret);
		object_init_ex(ret, Phurplebuddylist_ce);
		INIT_PZVAL(ret);

		zao = (struct ze_buddylist_obj *) zend_object_store_get_object(ret TSRMLS_CC);
		zao->pbuddylist = pbuddylist;
	}

	return ret;
}*/

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
	struct ze_group_obj *zgo;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|O", &buddy, PhurpleBuddy_ce, &group, PhurpleGroup_ce) == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(buddy TSRMLS_CC);
	zgo = (struct ze_group_obj *) zend_object_store_get_object(group TSRMLS_CC);

	purple_blist_add_buddy(zbo->pbuddy, NULL, zgo->pgroup, NULL);

	purple_blist_schedule_save();

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool PhurpleBuddyList::addGroup(string name)
	Adds new group to the blist */
PHP_METHOD(PhurpleBuddyList, addGroup)
{
	zval *group;
	struct ze_group_obj *zgo;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PhurpleGroup_ce) == FAILURE) {
		RETURN_NULL();
	}

	zgo = (struct ze_group_obj *) zend_object_store_get_object(group TSRMLS_CC);
	
	purple_blist_add_group(zgo->pgroup, NULL);

	purple_blist_schedule_save();

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto PhurpleBuddy PhurpleBuddyList::findBuddy(PhurpleAccount account, string name)
	returns the buddy, if found */
PHP_METHOD(PhurpleBuddyList, findBuddy)
{
	zval *account;
	char *name;
	php_size_t name_len;
	PurpleBuddy *pbuddy;
	struct ze_account_obj *zao;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os", &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

	pbuddy = purple_find_buddy(zao->paccount, name);

	if(pbuddy) {
		zval *buddy = php_create_buddy_obj_zval(pbuddy TSRMLS_CC);

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
		
		object_init_ex(return_value, PhurpleGroup_ce);
		
		fci.size = sizeof(fci);
		fci.function_table = EG(function_table);
		fci.function_name = NULL;
		fci.symbol_table = NULL;
		fci.retval_ptr_ptr = &retval_ptr;
		fci.param_count = 1;
		fci.params = params;
		fci.no_separation = 1;

		fcc.initialized = 1;
		fcc.function_handler = PhurpleGroup_ce->constructor;
		fcc.calling_scope = EG(scope);
		fcc.object_ptr = return_value;

		if (zend_call_function(&fci, &fcc TSRMLS_CC) == FAILURE) {
			efree(params);
			zval_ptr_dtor(&retval_ptr);
			zend_error(E_WARNING, "Invocation of %s's constructor failed", PhurpleGroup_ce->name);
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

	purple_blist_schedule_save();

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto boolean PhurpleBuddyList::removeGroup(PhurpleGroup group)
	Removes an empty group from the buddy list */
PHP_METHOD(PhurpleBuddyList, removeGroup)
{
	zval *group;
	struct ze_group_obj *zgo;
	PurpleBlistNode *node;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &group, PhurpleGroup_ce) == FAILURE) {
		RETURN_FALSE;
	}

	zgo = (struct ze_group_obj *) zend_object_store_get_object(group TSRMLS_CC);

	node = (PurpleBlistNode *) zgo->pgroup;

	if(node->child) {
		/* group isn't empty */
		RETURN_FALSE;
	}
	
	purple_blist_remove_group(zgo->pgroup);

	purple_blist_schedule_save();

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto boolean PhurpleBuddyList::addChat(string name, Phurple\Account account)
	Save chat into buddy list */
PHP_METHOD(PhurpleBuddyList, addChat)
{
	char *chat;
	php_size_t chat_len;
	GHashTable *components;
	zval *account;
	struct ze_account_obj *zao;
	PurpleChat *pchat;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sO", &chat, &chat_len, &account, PhurpleAccount_ce) == FAILURE) {
		RETURN_NULL();
	}

	zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("channel"), chat);

	pchat = purple_chat_new(zao->paccount, NULL, components);

	purple_blist_add_chat(pchat, NULL, NULL);

	purple_blist_schedule_save();
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
