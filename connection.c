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

extern zval *
php_create_account_obj_zval(PurpleAccount *paccount TSRMLS_DC);

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

extern zval*
phurple_long_zval(long l);

extern zval*
phurple_string_zval(const char *s);

void
php_connection_obj_destroy(void *obj TSRMLS_DC)
{/*{{{*/
	struct ze_connection_obj *zco = (struct ze_connection_obj *)obj;

	zend_object_std_dtor(&zco->zo TSRMLS_CC);

	/*if (zco->pconnection) {
		purple_connection_destroy(zco->pconnection);
	}*/

	efree(zco);
}/*}}}*/

zend_object_value
php_connection_obj_init(zend_class_entry *ce TSRMLS_DC)
{/*{{{*/
	zend_object_value ret;
	struct ze_connection_obj *zco;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zval *tmp;
#endif

	zco = (struct ze_connection_obj *) emalloc(sizeof(struct ze_connection_obj));
	memset(&zco->zo, 0, sizeof(zend_object));

	zend_object_std_init(&zco->zo, ce TSRMLS_CC);
#if PHP_MAJOR_VERSION== 5 && PHP_MINOR_VERSION < 4
	zend_hash_copy(zco->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref,
					(void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&zco->zo, ce);
#endif

	zco->pconnection = NULL;

	ret.handle = zend_objects_store_put(zco, NULL,
								(zend_objects_free_object_storage_t) php_connection_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}/*}}}*/

zval *
php_create_connection_obj_zval(PurpleConnection *pconnection TSRMLS_DC)
{/*{{{*/
	zval *ret;
	struct ze_connection_obj *zao;

	if (!pconnection) {
		ALLOC_INIT_ZVAL(ret);
		ZVAL_NULL(ret);
	} else {
		ALLOC_ZVAL(ret);
		object_init_ex(ret, PhurpleConnection_ce);
		INIT_PZVAL(ret);

		zao = (struct ze_connection_obj *) zend_object_store_get_object(ret TSRMLS_CC);
		zao->pconnection = pconnection;
	}

	return ret;
}/*}}}*/

/*
**
**
** Phurple connection methods
**
*/

/* {{{ proto object PhurpleConnection::__construct()
	constructor*/
PHP_METHOD(PhurpleConnection, __construct)
{
	/* XXX to implement here */	
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleConnection::getAccount()
		Returns the connection's account*/
PHP_METHOD(PhurpleConnection, getAccount)
{
	PurpleAccount *acc = NULL;
	struct ze_connection_obj *zco;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zco = (struct ze_connection_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if(NULL != zco->pconnection) {
		acc = purple_connection_get_account(zco->pconnection);
		if(NULL != acc) {
			zval *ret = php_create_account_obj_zval(acc TSRMLS_CC);

			*return_value = *ret;

			return;
		}
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto void Phurple\Connection::setAccount(Phurple\Account account)
	Sets the specified connection's phurple_account */
PHP_METHOD(PhurpleConnection, setAccount)
{
	zval *account;
	struct ze_connection_obj *zco;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &account, PhurpleAccount_ce) == FAILURE) {
		return;
	}
	
	zco = (struct ze_connection_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	if(zco->pconnection) {
			struct ze_account_obj *zao;
			zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);
			purple_connection_set_account(zco->pconnection, zao->paccount);
	}
}
/* }}} */

/*
**
**
** End phurple connection methods
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
