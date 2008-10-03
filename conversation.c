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
	PurpleAccount *paccount = NULL;
	GList *conversations = NULL, *cnv;
	zval *account, *account_index;
	gchar *name1;
	const gchar *name2;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lOs", &type, &account, PhurpleAccount_ce, &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	account_index = zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC);
	paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(account_index));

	if(NULL != account) {
		conv = purple_conversation_new(type, paccount, estrdup(name));
		conversations = purple_get_conversations();

		cnv = conversations;
		name1 = g_strdup(purple_normalize(paccount, name));

		for (; cnv != NULL; cnv = cnv->next) {
			name2 = purple_normalize(paccount, purple_conversation_get_name((PurpleConversation *)cnv->data));

			if ((paccount == purple_conversation_get_account((PurpleConversation *)cnv->data)) &&
					!purple_utf8_strcasecmp(name1, name2)) {
				conv_list_position = g_list_position(conversations, cnv);
			}
		}

		conv_list_position = conv_list_position == -1
		                     ? g_list_position(conversations, g_list_last(conversations))
		                     : conv_list_position;

		zend_update_property_long(PhurpleConversation_ce,
		                          getThis(),
		                          "index",
		                          sizeof("index")-1,
		                          (long)conv_list_position TSRMLS_CC
		                          );

		return;
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto string PhurpleConversation::getName(void)
	Returns the specified conversation's name*/
PHP_METHOD(PhurpleConversation, getName)
{
	zval *conversation_index;
	PurpleConversation *conversation = NULL;

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		RETURN_STRING(estrdup(purple_conversation_get_name(conversation)), 0);
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
	PurpleConversation *conversation = NULL;
	zval *conversation_index;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &message, &message_len) == FAILURE) {
		RETURN_NULL();
	}

	ALLOC_INIT_ZVAL(conversation_index);
	ZVAL_LONG(conversation_index, Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(conversation_index));

	if(NULL != conversation) {
		purple_conv_im_send(PURPLE_CONV_IM(conversation), estrdup(message));
	}
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleConversation::getAccount(void)
	Gets the account of this conversation*/
PHP_METHOD(PhurpleConversation, getAccount)
{
	PurpleConversation *conversation = NULL;
	PurpleAccount *acc = NULL;
	zval *conversation_index;

	conversation = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	if(NULL != conversation) {
		acc = purple_conversation_get_account(conversation);
		if(NULL != acc) {
			ZVAL_NULL(return_value);
			Z_TYPE_P(return_value) = IS_OBJECT;
			object_init_ex(return_value, PhurpleAccount_ce);
			zend_update_property_long(PhurpleAccount_ce,
			                          return_value,
			                          "index",
			                          sizeof("index")-1,
			                          (long)g_list_position(purple_accounts_get_all(),
			                          g_list_find(purple_accounts_get_all(), acc)) TSRMLS_CC
			                          );
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
	PurpleConversation *pconv = NULL;
	PurpleAccount *paccount = NULL;
	zval *account;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &account, PhurpleAccount_ce) == FAILURE) {
		RETURN_NULL();
	}
	
	pconv = g_list_nth_data (purple_get_conversations(), (guint)Z_LVAL_P(zend_read_property(PhurpleConversation_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	
	if(pconv) {
		paccount = g_list_nth_data (purple_accounts_get_all(), (guint)Z_LVAL_P(zend_read_property(PhurpleAccount_ce, account, "index", sizeof("index")-1, 0 TSRMLS_CC)));
		
		if(account) {
			purple_conversation_set_account(pconv, paccount);
		}
	}
}
/* }}} */

/*
**
**
** End phurple conversation methods
**
*/
