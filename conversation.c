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

extern zval *
php_create_connection_obj_zval(PurpleConnection *pconnection TSRMLS_DC);

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

	ret.handle = zend_objects_store_put(zao, NULL,
								(zend_objects_free_object_storage_t) php_conversation_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}/*}}}*/

zval *
php_create_conversation_obj_zval(PurpleConversation *pconv TSRMLS_DC)
{/*{{{*/
	zval *ret;
	struct ze_conversation_obj *zco;

	ALLOC_ZVAL(ret);
	object_init_ex(ret, PhurpleConversation_ce);
	INIT_PZVAL(ret);

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(ret TSRMLS_CC);
	zco->pconversation = pconv;


	return ret;
}/*}}}*/


static gboolean
phurple_writing_im_msg(PurpleAccount *account, const char *who, char **message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/
	//printf(" writing XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	return 0;
}/*}}}*/

static void
phurple_wrote_im_msg(PurpleAccount *account, const char *who, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/
	//printf(" wrote XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
}/*}}}*/

static void
phurple_sending_im_msg(PurpleAccount *account, const char *receiver, char **message)
{/*{{{*/
	//printf(" sending XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");

}/*}}}*/

static void
phurple_sent_im_msg(PurpleAccount *account, const char *receiver, const char *message)
{/*{{{*/
	//printf(" sent XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");

}/*}}}*/

static gboolean
phurple_receiving_im_msg(PurpleAccount *account, char **sender, char **message, PurpleConversation *conv, PurpleMessageFlags *flags)
{/*{{{*/
	//printf(" receiving XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	return 0;
}/*}}}*/

static void
phurple_received_im_msg(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/
	//printf(" received XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");

}/*}}}*/

static gboolean 
phurple_writing_chat_msg(PurpleAccount *account, const char *who, char **message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/
	return 0;
}/*}}}*/

static void
phurple_wrote_chat_msg(PurpleAccount *account, const char *who, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_sending_chat_msg(PurpleAccount *account, char **message, int id)
{/*{{{*/

}/*}}}*/

static void
phurple_sent_chat_msg(PurpleAccount *account, const char *message, int id)
{/*{{{*/
	
}/*}}}*/

static gboolean
phurple_receiving_chat_msg(PurpleAccount *account, char **sender, char **message, PurpleConversation *conv, int *flags)
{/*{{{*/
	return 0;
}/*}}}*/

static void
phurple_blocked_im_msg(PurpleAccount *account, const char *sender, const char *message, PurpleMessageFlags flags, time_t when)
{/*{{{*/

}/*}}}*/

static void
phurple_received_chat_msg(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{/*{{{*/

}/*}}}*/

static void
phurple_conversation_created(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_conversation_updated(PurpleConversation *conv, PurpleConvUpdateType type)
{/*{{{*/

}/*}}}*/

static void
phurple_deleting_conversation(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_buddy_typing(PurpleAccount *account, const char *name)
{/*{{{*/

}/*}}}*/

static void
phurple_buddy_typing_stopped(PurpleAccount *account, const char *name)
{/*{{{*/

}/*}}}*/

static gboolean
phurple_chat_buddy_joining(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags flags)
{/*{{{*/
	return 0;
}/*}}}*/

static void
phurple_chat_buddy_joined(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags flags, gboolean new_arrival)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_join_failed(PurpleConnection *gc, GHashTable *components)
{/*{{{*/

}/*}}}*/

static gboolean
phurple_chat_buddy_leaving(PurpleConversation *conv, const char *name, const char *reason)
{/*{{{*/
	return 0;
}/*}}}*/

static void
phurple_chat_buddy_left(PurpleConversation *conv, const char *name, const char *reason)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_inviting_user(PurpleConversation *conv, const char *name, char **invite_message)
{/*{{{*/

}/*}}}*/

static void 
phurple_chat_invited_user(PurpleConversation *conv, const char *name, const char *invite_message)
{/*{{{*/

}/*}}}*/

static gint
phurple_chat_invited(PurpleAccount *account, const char *inviter, const char *chat, const char *invite_message, const GHashTable *components)
{/*{{{*/
	return 0;
}/*}}}*/

static void 
phurple_chat_invite_blocked(PurpleAccount *account, const char *inviter, const char *name, const char *message, GHashTable *data)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_joined(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_left(PurpleConversation *conv)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_topic_changed(PurpleConversation *conv, const char *who, const char *topic)
{/*{{{*/

}/*}}}*/

static void
phurple_chat_buddy_flags(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags oldflags, PurpleConvChatBuddyFlags newflags)
{/*{{{*/

}/*}}}*/

void
phurple_setup_conv_signals(PurpleConversation *conv)
{

	purple_signal_connect(purple_conversations_get_handle(),
						  "writing-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_writing_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "wrote-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_wrote_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "sending-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_sending_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "sent-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_sent_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "receiving-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_receiving_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "received-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_received_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "blocked-im-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_blocked_im_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "writing-chat-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_writing_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "wrote-chat-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_wrote_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "sending-chat-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_sending_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "sent-chat-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_sent_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "receiving-chat-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_receiving_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "received-chat-msg",
						  conv,
						  PURPLE_CALLBACK(phurple_received_chat_msg),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "conversation-created",
						  conv,
						  PURPLE_CALLBACK(phurple_conversation_created),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "conversation-updated",
						  conv,
						  PURPLE_CALLBACK(phurple_conversation_updated),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "deleting-conversation",
						  conv,
						  PURPLE_CALLBACK(phurple_deleting_conversation),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "buddy-typing",
						  conv,
						  PURPLE_CALLBACK(phurple_buddy_typing),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "buddy-typing-stopped",
						  conv,
						  PURPLE_CALLBACK(phurple_buddy_typing_stopped),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-buddy-joined",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_buddy_joined),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-buddy-joined",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_buddy_joined),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-buddy-leaving",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_buddy_leaving),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-buddy-left",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_buddy_left),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-inviting-user",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_inviting_user),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-invited-user",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_invited_user),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-invited",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_invited),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-invite-blocked",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_invite_blocked),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-joined",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_joined),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-join-failed",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_join_failed),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-left",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_left),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-topic-changed",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_topic_changed),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-buddy-flags",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_buddy_flags),
						  NULL
	);

	purple_signal_connect(purple_conversations_get_handle(),
						  "chat-buddy-joining",
						  conv,
						  PURPLE_CALLBACK(phurple_chat_buddy_joining),
						  NULL
	);
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

	switch (type) {
		case PURPLE_CONV_TYPE_IM:
		case PURPLE_CONV_TYPE_CHAT:
			zco->pconversation = purple_find_conversation_with_account(type, name, zao->paccount);
			if (!zco->pconversation) {
				zco->pconversation = purple_conversation_new(type, zao->paccount, name);
			}
			break;

		default:
			zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Unknown conversation type");
			return;
	}

	if (NULL == zco->pconversation) {
		zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Failed to create conversation");
		return;
	}

	phurple_setup_conv_signals(zco->pconversation);

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

		pchat = purple_blist_find_chat(zao->paccount, name);
		if (!pchat) {
			pchat = purple_chat_new(zao->paccount, name, components);
		}
		//purple_blist_node_set_flags((PurpleBlistNode *)pchat, PURPLE_BLIST_NODE_FLAG_NO_SAVE);
		//
		serv_join_chat(purple_account_get_connection(zao->paccount), components);
		//purple_conversation_present(zco->pconversation);
		//
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
		switch (purple_conversation_get_type(zco->pconversation)) {
			case PURPLE_CONV_TYPE_IM:
				purple_conv_im_send(PURPLE_CONV_IM(zco->pconversation), message);
				break;

			case PURPLE_CONV_TYPE_CHAT:
				purple_conv_chat_send(PURPLE_CONV_CHAT(zco->pconversation), message);
				break;

			default:
				zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Unknown conversation type");
				return;
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

/* {{{ proto void Phurple\Conversation::setAccount(Phurple\Account account)
	Sets the specified conversation's phurple_account */
PHP_METHOD(PhurpleConversation, setAccount)
{
	zval *account;
	struct ze_conversation_obj *zco;
	
	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &account, PhurpleAccount_ce) == FAILURE) {
		return;
	}
	
	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);
	
	if(zco->pconversation) {
			struct ze_account_obj *zao;
			zao = (struct ze_account_obj *) zend_object_store_get_object(account TSRMLS_CC);
			purple_conversation_set_account(zco->pconversation, zao->paccount);
	}
}
/* }}} */

/* {{{ proto void Phurple\Conversation::inviteUser(string user, string message)
	Invite a user to a chat */
PHP_METHOD(PhurpleConversation, inviteUser)
{
	struct ze_conversation_obj *zco;
	char *user, *msg;
	int user_len, msg_len;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &user, &user_len, &msg, &msg_len) == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if(NULL != zco->pconversation) {
		switch (purple_conversation_get_type(zco->pconversation)) {
			case PURPLE_CONV_TYPE_CHAT:
				purple_conv_chat_invite_user(PURPLE_CONV_CHAT(zco->pconversation), user, msg, 0);
				break;

			default:
				zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Initialized conversation type doesn't support invitations");
				return;
		}
	}
}
/* }}} */

/* {{{ proto boolean Phurple\Conversation::isUserInChat(string user)
	Lookup user in chat */
PHP_METHOD(PhurpleConversation, isUserInChat)
{
	struct ze_conversation_obj *zco;
	char *user;
	int user_len;
	gboolean ret;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user, &user_len) == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if(NULL != zco->pconversation) {
		switch (purple_conversation_get_type(zco->pconversation)) {
			case PURPLE_CONV_TYPE_CHAT:
				ret = purple_conv_chat_find_user(PURPLE_CONV_CHAT(zco->pconversation), user);
				break;

			default:
				zend_throw_exception_ex(PhurpleException_ce, 0 TSRMLS_CC, "Initialized conversation type doesn't support invitations");
				return;
		}
	}

	RETVAL_BOOL((long)ret);
}
/* }}} */

/* {{{ proto public Phurple\Connection Phurple\Conversation::getConnection(void)
	Get the connection object corresponding to the conversation */
PHP_METHOD(PhurpleConversation, getConnection)
{
	struct ze_conversation_obj *zco;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if(NULL != zco->pconversation) {
		const PurpleConnection *pconn = purple_conversation_get_connection(zco->pconversation);
		if (pconn) {
			zval *tmp = php_create_connection_obj_zval(pconn TSRMLS_CC);

			*return_value = *tmp;

			return;
		}
	}

	RETURN_NULL();
}
/* }}} */


/* {{{ proto public void Phurple\Conversation::setTitle(string title)
	Set conversation title */
PHP_METHOD(PhurpleConversation, setTitle)
{
	struct ze_conversation_obj *zco;
	char *title;
	int title_len;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &title, &title_len) == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if (zco->pconversation) {
		purple_conversation_set_name(zco->pconversation, title);
	}
}
/* }}} */

/* {{{ proto public string Phurple\Conversation::setTitle(void)
	Set conversation title */
PHP_METHOD(PhurpleConversation, getTitle)
{
	struct ze_conversation_obj *zco;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	zco = (struct ze_conversation_obj *) zend_object_store_get_object(getThis() TSRMLS_CC);

	if (zco->pconversation) {
		RETVAL_STRING(purple_conversation_get_title(zco->pconversation), 1);
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
