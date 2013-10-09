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

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

void
php_buddy_obj_destroy(void *obj TSRMLS_DC)
{
	struct ze_buddy_obj *zbo = (struct ze_buddy_obj *)obj;

	zend_object_std_dtor(&zbo->zo TSRMLS_CC);

	/*if (zbo->pbuddy) {
		purple_buddy_destroy(zbo->pbuddy);
	}*/

	efree(zbo);
}

zend_object_value
php_buddy_obj_init(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ret;
	struct ze_buddy_obj *zbo;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zval *tmp;
#endif

	zbo = (struct ze_buddy_obj *) emalloc(sizeof(struct ze_buddy_obj));
	memset(&zbo->zo, 0, sizeof(zend_object));

	zend_object_std_init(&zbo->zo, ce TSRMLS_CC);
#if PHP_MAJOR_VERSION== 5 && PHP_MINOR_VERSION < 4
	zend_hash_copy(zbo->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref,
					(void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&zbo->zo, ce);
#endif

	zbo->pbuddy = NULL;

	ret.handle = zend_objects_store_put(zbo, NULL,
								(zend_objects_free_object_storage_t) php_buddy_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}

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
	char *name, *alias = NULL;
	int name_len, alias_len = 0;
	zval *account;
	struct ze_buddy_obj *zbo;
	struct ze_account_obj *zao;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Os|s", &account, PhurpleAccount_ce, &name, &name_len, &alias, &alias_len) == FAILURE) {
		RETURN_NULL();
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

	zbo->pbuddy = purple_find_buddy(zao->paccount, name);

	if(!zbo->pbuddy) {
		zbo->pbuddy = purple_buddy_new(zao->paccount, name, alias_len ? alias : name);
	}

	if (NULL == zbo->pbuddy) {
		zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Failed to create buddy");
		return;
	}
}
/* }}} */


/* {{{ proto string PhurpleBuddy::getName(void)
	Gets buddy name*/
PHP_METHOD(PhurpleBuddy, getName)
{
	struct ze_buddy_obj *zbo;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	RETURN_STRING(purple_buddy_get_name(zbo->pbuddy), 1);
}
/* }}} */


/* {{{ proto string PhurpleBuddy::getAlias(void)
	gets buddy alias */
PHP_METHOD(PhurpleBuddy, getAlias)
{
	struct ze_buddy_obj *zbo;
	const char *alias;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	alias = purple_buddy_get_alias_only(zbo->pbuddy);

	if (alias) {
		RETURN_STRING(alias, 1);
	}
}
/* }}} */


/* {{{ proto PhurpleGroup PhurpleBuddy::getGroup(void)
	gets buddy's group */
PHP_METHOD(PhurpleBuddy, getGroup)
{
	PurpleGroup *pgroup = NULL;
	struct ze_buddy_obj *zbo;
			
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);


	pgroup = purple_buddy_get_group(zbo->pbuddy);
	if(pgroup) {
		zval *tmp;
		struct ze_buddygroup_obj *zgo;

		PHURPLE_MK_OBJ(tmp, PhurpleBuddyGroup_ce);

		zgo = (struct ze_buddygroup_obj *) zend_object_store_get_object(tmp TSRMLS_CC);

		zgo->pbuddygroup = pgroup;

		*return_value = *tmp;

		return;
	}

	RETURN_NULL();
	
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleBuddy::getAccount(void)
	gets buddy's account */
PHP_METHOD(PhurpleBuddy, getAccount)
{
	PurpleAccount *paccount = NULL;
	struct ze_buddy_obj *zbo;
			
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
			
	paccount = purple_buddy_get_account(zbo->pbuddy);
	if(paccount) {
		struct ze_account_obj *zao;

		PHURPLE_MK_OBJ(return_value, PhurpleAccount_ce);

		zao = (struct ze_account_obj *) zend_object_store_get_object(return_value TSRMLS_CC);

		zao->paccount = paccount;

		return;
	}

	RETURN_NULL();
}
/* }}} */



/* {{{ proto bool PhurpleBuddy::isOnline(void)
	checks weither the buddy is online */
PHP_METHOD(PhurpleBuddy, isOnline)
{
	struct ze_buddy_obj *zbo;
			
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zbo = (struct ze_buddy_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
			
	RETVAL_BOOL(PURPLE_BUDDY_IS_ONLINE(zbo->pbuddy));
}
/* }}} */

/*
**
**
** End phurple Buddy methods
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
