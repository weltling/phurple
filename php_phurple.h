/**
 * Copyright (c) 2007-2013, Anatol Belski <ab@php.net>
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

#ifndef PHP_PHURPLE_H
#define PHP_PHURPLE_H

#define PHP_PHURPLE_VERSION "0.6.0"

extern zend_module_entry phurple_module_entry;
#define phpext_phurple_ptr &phurple_module_entry

#ifdef PHP_WIN32
# define PHP_PHURPLE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_PHURPLE_API __attribute__ ((visibility("default")))
#else
# define PHP_PHURPLE_API
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
PHP_METHOD(PhurpleAccount, getPresence);
PHP_METHOD(PhurpleAccount, setStatus);

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
PHP_METHOD(PhurpleBuddyList, addChat);

PHP_METHOD(PhurpleBuddyGroup, __construct);
PHP_METHOD(PhurpleBuddyGroup, getAccounts);
PHP_METHOD(PhurpleBuddyGroup, getSize);
PHP_METHOD(PhurpleBuddyGroup, getOnlineCount);
PHP_METHOD(PhurpleBuddyGroup, getName);

PHP_METHOD(PhurplePresence, __construct);

ZEND_BEGIN_MODULE_GLOBALS(phurple)

	/**
	 * This are ini settings
	 */
	char *custom_plugin_path;

	/**
	 * Client singleton instance
	 */
	zval *phurple_client_obj;

ZEND_END_MODULE_GLOBALS(phurple)

/**
 * Signal names
 */
#define SIGNAL_SIGNED_ON "signed-on"
#define SIGNAL_SIGNED_OFF "signed-off"

#ifdef ZTS
#define PHURPLE_G(v) TSRMG(phurple_globals_id, zend_phurple_globals *, v)
#else
#define PHURPLE_G(v) (phurple_globals.v)
#endif

ZEND_EXTERN_MODULE_GLOBALS(phurple)

#define PHURPLE_MK_OBJ(o, c) MAKE_STD_ZVAL(o); object_init_ex(o, c);

#define PHURPLE_INTERNAL_DEBUG 0

extern zend_class_entry *PhurpleClient_ce;
extern zend_class_entry *PhurpleConversation_ce;
extern zend_class_entry *PhurpleAccount_ce;
extern zend_class_entry *PhurpleConnection_ce;
extern zend_class_entry *PhurpleBuddy_ce;
extern zend_class_entry *PhurpleBuddyList_ce;
extern zend_class_entry *PhurpleBuddyGroup_ce;
extern zend_class_entry *PhurpleException_ce;
extern zend_class_entry *PhurplePresence_ce;

# define PHURPLE_CLIENT_CLASS_NAME "Phurple\\Client"
# define PHURPLE_CONVERSATION_CLASS_NAME "Phurple\\Conversation"
# define PHURPLE_ACCOUNT_CLASS_NAME "Phurple\\Account"
# define PHURPLE_CONNECION_CLASS_NAME "Phurple\\Connection"
# define PHURPLE_BUDDY_CLASS_NAME "Phurple\\Buddy"
# define PHURPLE_BUDDYLIST_CLASS_NAME "Phurple\\BuddyList"
# define PHURPLE_BUDDY_GROUP_CLASS_NAME "Phurple\\BuddyGroup"
# define PHURPLE_EXCEPTION_CLASS_NAME "Phurple\\Exception"
# define PHURPLE_PRESENCE_CLASS_NAME "Phurple\\Presence"

struct ze_buddy_obj {
	zend_object zo;
	PurpleBuddy *pbuddy;
};

struct ze_buddygroup_obj {
	zend_object zo;
	PurpleGroup *pbuddygroup;
};

struct ze_account_obj {
	zend_object zo;
	PurpleAccount *paccount;
};

struct ze_conversation_obj {
	zend_object zo;
	PurpleConversation *pconversation;
	PurpleConversationType ptype;
};

struct ze_connection_obj {
	zend_object zo;
	PurpleConnection *pconnection;
};

struct ze_client_obj {
	zend_object zo;
	int connection_handle;
	zend_class_entry *ce;
};

struct ze_presence_obj {
	zend_object zo;
	PurplePresence *ppresence;
};

zend_object_handlers default_phurple_obj_handlers;

#endif	/* PHP_PHURPLE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
