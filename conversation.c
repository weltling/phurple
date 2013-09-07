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

void
php_conversation_obj_destroy(void *obj TSRMLS_DC)
{
	struct ze_conversation_obj *zao = (struct ze_conversation_obj *)obj;

	zend_object_std_dtor(&zao->zo TSRMLS_CC);

	efree(zao);
}

zend_object_value
php_conversation_obj_init(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ret;
	struct ze_conversation_obj *zao;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zval *tmp;
#endif

	zao = (struct ze_conversation_obj *) emalloc(sizeof(struct ze_conversation_obj));
	memset(&zao->zo, 0, sizeof(zend_object));

	zend_object_std_init(&zao->zo, ce TSRMLS_CC);
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zend_hash_copy(zao->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref,
					(void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&zao->zo, ce);
#endif

	zao->pconversation = NULL;

	ret.handle = zend_objects_store_put(zao, NULL,
								(zend_objects_free_object_storage_t) php_conversation_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}


/*
**
**
** Phurple conversation methods
**
*/

/* {{{ proto int PhurpleConversation::__construct(int type, PhurpleAccount account, string name)
	Creates a new conversation of the specified type */
PHP_METHOD(PhurpleConversation, __construct)
{
	int type, name_len, conv_list_position = -1;
	char *name;
	PurpleConversation *conv = NULL;
	GList *conversations = NULL, *cnv;
	zval *account;
	gchar *name1;
	const gchar *name2;
	struct ze_account_obj *zao;
	struct ze_conversation_obj *zco;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lOs", &type, &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

	zco->pconversation = purple_conversation_new(type, zao->paccount, estrdup(name));

	if (NULL == zco->pconversation) {
		zend_throw_exception_ex(PhurpleException_ce, "Failed to create conversation", 0 TSRMLS_CC);
		return;
	}
}
/* }}} */


/* {{{ proto string PhurpleConversation::getName(void)
	Returns the specified conversation's name*/
PHP_METHOD(PhurpleConversation, getName)
{
	struct ze_conversation_obj *zco;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if(NULL != zco->pconversation) {
		const gchar *name = purple_conversation_get_name(zco->pconversation);
		if (name) {
			RETURN_STRING(name, 1);
		}
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto void PhurpleConversation::sendIM(string message)
	Sends a message to this IM conversation */
PHP_METHOD(PhurpleConversation, sendIM)
{
	int message_len;
	char *message;
	struct ze_conversation_obj *zco;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &message, &message_len) == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if(message_len && NULL != zco->pconversation) {
		purple_conv_im_send(PURPLE_CONV_IM(zco->pconversation), estrdup(message));
	}
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleConversation::getAccount(void)
	Gets the account of this conversation*/
PHP_METHOD(PhurpleConversation, getAccount)
{
	PurpleAccount *acc = NULL;
	struct ze_conversation_obj *zco;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	if(NULL != zco->pconversation) {
		acc = purple_conversation_get_account(zco->pconversation);
		if(NULL != acc) {
			zval *ret = php_create_account_obj_zval(acc TSRMLS_CC);

			*return_value = *ret;

			return;
		}
	}

	RETURN_NULL();
}
/* }}} */

/* {{{ proto void PhurpleConversation::setAccount(PhurpleAccount account)
	Sets the specified conversation's phurple_account */
PHP_METHOD(PhurpleConversation, setAccount)
{
	zval *account;
	struct ze_account_obj *zao;
	struct ze_conversation_obj *zco;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &account, PhurpleAccount_ce) == FAILURE) {
		RETURN_NULL();
	}
	
	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	if(zco->pconversation) {
			struct ze_account_obj *zao;
			zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);
			purple_conversation_set_account(zco->pconversation, zao->paccount);
	}
}
/* }}} */

/*
**
**
** End phurple conversation methods
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
