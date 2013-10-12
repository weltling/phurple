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

extern zval *
php_create_account_obj_zval(PurpleAccount *paccount TSRMLS_DC);

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

void
php_conversation_obj_destroy(void *obj TSRMLS_DC)
{/*{{{*/
	struct ze_conversation_obj *zao = (struct ze_conversation_obj *)obj;

	zend_object_std_dtor(&zao->zo TSRMLS_CC);

	efree(zao);
}/*}}}*/

zend_object_value
php_conversation_obj_init(zend_class_entry *ce TSRMLS_DC)
{/*{{{*/
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
	zao->ptype = PURPLE_CONV_TYPE_UNKNOWN;

	ret.handle = zend_objects_store_put(zao, NULL,
								(zend_objects_free_object_storage_t) php_conversation_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}/*}}}*/

zval *
php_create_conversation_obj_zval(PurpleConversation *pconv, PurpleConversationType ptype TSRMLS_DC)
{/*{{{*/
	zval *ret;
	struct ze_conversation_obj *zco;

	ALLOC_ZVAL(ret);
	object_init_ex(ret, PhurpleConversation_ce);
	INIT_PZVAL(ret);

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(ret TSRMLS_CC);
	zco->pconversation = pconv;
	zco->ptype = ptype;


	return ret;
}/*}}}*/

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
	int type, name_len;
	char *name;
	zval *account;
	struct ze_account_obj *zao;
	struct ze_conversation_obj *zco;
	PurpleChat *pchat = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lOs", &type, &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);

	zco->pconversation = purple_conversation_new(type, zao->paccount, name);
	zco->ptype = type;

	if (NULL == zco->pconversation) {
		zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Failed to create conversation");
		return;
	}

	pchat = purple_blist_find_chat(zao->paccount, name);
	if (!pchat) {
		GHashTable *components;
		PurplePlugin *prpl = purple_find_prpl(purple_account_get_protocol_id(zao->paccount));
		PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

		if (purple_account_get_connection(zao->paccount) != NULL &&
			PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, chat_info_defaults)) {
				components = prpl_info->chat_info_defaults(purple_account_get_connection(zao->paccount),
				purple_conversation_get_name(zco->pconversation)
			);
		} else {
			components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
			g_hash_table_replace(components, g_strdup("channel"), g_strdup(name));
		}

		pchat = purple_chat_new(zao->paccount, NULL, components);
		//purple_blist_node_set_flags((PurpleBlistNode *)pchat, PURPLE_BLIST_NODE_FLAG_NO_SAVE);
		//
		serv_join_chat(purple_account_get_connection(zao->paccount), components);
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
		switch (zco->ptype) {
			case PURPLE_CONV_TYPE_IM:
				purple_conv_im_send(PURPLE_CONV_IM(zco->pconversation), message);
				break;

			case PURPLE_CONV_TYPE_CHAT:
				purple_conv_chat_send(PURPLE_CONV_CHAT(zco->pconversation), message);
				break;
		}
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
