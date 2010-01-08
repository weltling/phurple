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

#ifndef PHP_PHURPLE_H
#define PHP_PHURPLE_H

extern zend_module_entry phurple_module_entry;
#define phpext_phurple_ptr &phurple_module_entry

#ifdef PHP_WIN32
#define PHP_PHURPLE_API __declspec(dllexport)
#else
#define PHP_PHURPLE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#include <purple.h>

PHP_MINIT_FUNCTION(phurple);
PHP_MSHUTDOWN_FUNCTION(phurple);
PHP_RINIT_FUNCTION(phurple);
PHP_RSHUTDOWN_FUNCTION(phurple);
PHP_MINFO_FUNCTION(phurple);

PHP_METHOD(PhurpleClient, __construct);
PHP_METHOD(PhurpleClient, getInstance);
PHP_METHOD(PhurpleClient, initInternal);
PHP_METHOD(PhurpleClient, getCoreVersion);
PHP_METHOD(PhurpleClient, writeConv);
PHP_METHOD(PhurpleClient, writeIM);
PHP_METHOD(PhurpleClient, onSignedOn);
/*PHP_METHOD(PhurpleClient, onSignedOff);*/
PHP_METHOD(PhurpleClient, runLoop);
PHP_METHOD(PhurpleClient, addAccount);
PHP_METHOD(PhurpleClient, getProtocols);
PHP_METHOD(PhurpleClient, loopCallback);
PHP_METHOD(PhurpleClient, loopHeartBeat);
PHP_METHOD(PhurpleClient, deleteAccount);
PHP_METHOD(PhurpleClient, findAccount);
PHP_METHOD(PhurpleClient, authorizeRequest);
PHP_METHOD(PhurpleClient, iterate);
/*PHP_METHOD(PhurpleClient, set);
PHP_METHOD(PhurpleClient, get);*/
PHP_METHOD(PhurpleClient, connect);
/*PHP_METHOD(PhurpleClient, disconnect);*/
PHP_METHOD(PhurpleClient, setUserDir);
PHP_METHOD(PhurpleClient, setDebug);
PHP_METHOD(PhurpleClient, setUiId);
PHP_METHOD(PhurpleClient, __clone);



PHP_METHOD(PhurpleAccount, __construct);
PHP_METHOD(PhurpleAccount, setPassword);
PHP_METHOD(PhurpleAccount, setEnabled);
PHP_METHOD(PhurpleAccount, addBuddy);
PHP_METHOD(PhurpleAccount, removeBuddy);
PHP_METHOD(PhurpleAccount, clearSettings);
PHP_METHOD(PhurpleAccount, set);
PHP_METHOD(PhurpleAccount, get);
PHP_METHOD(PhurpleAccount, isConnected);
PHP_METHOD(PhurpleAccount, isConnecting);
PHP_METHOD(PhurpleAccount, getUserName);
PHP_METHOD(PhurpleAccount, getPassword);

PHP_METHOD(PhurpleConnection, __construct);
PHP_METHOD(PhurpleConnection, getAccount);

PHP_METHOD(PhurpleConversation, __construct);
PHP_METHOD(PhurpleConversation, getName);
PHP_METHOD(PhurpleConversation, sendIM);
PHP_METHOD(PhurpleConversation, getAccount);
PHP_METHOD(PhurpleConversation, setAccount);

PHP_METHOD(PhurpleBuddy, __construct);
PHP_METHOD(PhurpleBuddy, getName);
PHP_METHOD(PhurpleBuddy, getAlias);
PHP_METHOD(PhurpleBuddy, getGroup);
PHP_METHOD(PhurpleBuddy, getAccount);
PHP_METHOD(PhurpleBuddy, isOnline);

PHP_METHOD(PhurpleBuddyList, __construct);
PHP_METHOD(PhurpleBuddyList, addBuddy);
PHP_METHOD(PhurpleBuddyList, addGroup);
PHP_METHOD(PhurpleBuddyList, findBuddy);
PHP_METHOD(PhurpleBuddyList, load);
PHP_METHOD(PhurpleBuddyList, findGroup);
PHP_METHOD(PhurpleBuddyList, removeBuddy);
PHP_METHOD(PhurpleBuddyList, removeGroup);

PHP_METHOD(PhurpleBuddyGroup, __construct);
PHP_METHOD(PhurpleBuddyGroup, getAccounts);
PHP_METHOD(PhurpleBuddyGroup, getSize);
PHP_METHOD(PhurpleBuddyGroup, getOnlineCount);
PHP_METHOD(PhurpleBuddyGroup, getName);

ZEND_BEGIN_MODULE_GLOBALS(phurple)

	/**
	 * This are ini settings
	 */
	char *custom_plugin_path;

	/**
	 * This are instance specific settings
	 */
	char *custom_user_dir;
	char *ui_id;
	int debug;

	int connection_handle;

	/**
	 * @todo move the phurple_client_obj into the ppos struct
	 */
	zval *phurple_client_obj;

	/**
	 * php phurple object storage
	 */
	struct phurple_object_storage
	{
		HashTable buddy;
		HashTable group;
	} ppos;
ZEND_END_MODULE_GLOBALS(phurple)

/**
 * Signal names
 */
#define SIGNAL_SIGNED_ON "signed-on"
#define SIGNAL_SIGNED_OFF "signed-off"

/**
 * At the moment we only take care about PHP versions 5.3 or 5.2,
 * mostly because of namespaces. 
 */
#define USING_PHP_53 ZEND_MODULE_API_NO >= 20071006

#ifdef ZTS
#define PHURPLE_G(v) TSRMG(phurple_globals_id, zend_phurple_globals *, v)
#else
#define PHURPLE_G(v) (phurple_globals.v)
#endif

ZEND_EXTERN_MODULE_GLOBALS(phurple)

/**
 * @todo At many places this macros was used as follows:
 * PHURPLE_MK_OBJ(return_value, PhurpleAccount_ce);
 * But it doesn't really affect the return_value
 * Changing the MAKE_STD_ZVAL to ZVAL_NULL does work, why?
 */
#define PHURPLE_MK_OBJ(o, c) MAKE_STD_ZVAL(o); Z_TYPE_P(o) = IS_OBJECT; object_init_ex(o, c);

#define PHURPLE_INTERNAL_DEBUG 0

extern zend_class_entry *PhurpleClient_ce;
extern zend_class_entry *PhurpleConversation_ce;
extern zend_class_entry *PhurpleAccount_ce;
extern zend_class_entry *PhurpleConnection_ce;
extern zend_class_entry *PhurpleBuddy_ce;
extern zend_class_entry *PhurpleBuddyList_ce;
extern zend_class_entry *PhurpleBuddyGroup_ce;

#if USING_PHP_53
#define PHURPLE_CLIENT_CLASS_NAME "Phurple\Client"
#else
#define PHURPLE_CLIENT_CLASS_NAME "PhurpleClient"
#endif

#if USING_PHP_53
#define PHURPLE_CONVERSATION_CLASS_NAME "Phurple\Conversation"
#else
#define PHURPLE_CONVERSATION_CLASS_NAME "PhurpleConversation"
#endif

#if USING_PHP_53
#define PHURPLE_ACCOUNT_CLASS_NAME "Phurple\Account"
#else
#define PHURPLE_ACCOUNT_CLASS_NAME "PhurpleAccount"
#endif

#if USING_PHP_53
#define PHURPLE_CONNECION_CLASS_NAME "Phurple\Connection"
#else
#define PHURPLE_CONNECION_CLASS_NAME "PhurpleConnection"
#endif

#if USING_PHP_53
#define PHURPLE_BUDDY_CLASS_NAME "Phurple\Buddy"
#else
#define PHURPLE_BUDDY_CLASS_NAME "PhurpleBuddy"
#endif

#if USING_PHP_53
#define PHURPLE_BUDDYLIST_CLASS_NAME "Phurple\BuddyList"
#else
#define PHURPLE_BUDDYLIST_CLASS_NAME "PhurpleBuddyList"
#endif

#if USING_PHP_53
#define PHURPLE_BUDDY_GROUP_CLASS_NAME "Phurple\BuddyGroup"
#else
#define PHURPLE_BUDDY_GROUP_CLASS_NAME "PhurpleBuddyGroup"
#endif

#endif	/* PHP_PHURPLE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
