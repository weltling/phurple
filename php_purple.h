/**
 * Copyright (c) 2007-2008, Anatoliy Belsky
 *
 * This file is part of PHPurple.
 *
 * PHPurple is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHPurple is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHPurple.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PHP_PURPLE_H
#define PHP_PURPLE_H

extern zend_module_entry purple_module_entry;
#define phpext_purple_ptr &purple_module_entry

#ifdef PHP_WIN32
#define PHP_PURPLE_API __declspec(dllexport)
#else
#define PHP_PURPLE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(purple);
PHP_MSHUTDOWN_FUNCTION(purple);
PHP_RINIT_FUNCTION(purple);
PHP_RSHUTDOWN_FUNCTION(purple);
PHP_MINFO_FUNCTION(purple);

PHP_METHOD(PurpleClient, __construct);
PHP_METHOD(PurpleClient, getInstance);
PHP_METHOD(PurpleClient, initInternal);
PHP_METHOD(PurpleClient, getCoreVersion);
PHP_METHOD(PurpleClient, connectToSignal);
PHP_METHOD(PurpleClient, writeConv);
PHP_METHOD(PurpleClient, writeIM);
PHP_METHOD(PurpleClient, onSignedOn);
PHP_METHOD(PurpleClient, runLoop);
PHP_METHOD(PurpleClient, addAccount);
PHP_METHOD(PurpleClient, getProtocols);
PHP_METHOD(PurpleClient, setUserDir);
PHP_METHOD(PurpleClient, loopCallback);
PHP_METHOD(PurpleClient, loopHeartBeat);
PHP_METHOD(PurpleClient, deleteAccount);
PHP_METHOD(PurpleClient, findAccount);
PHP_METHOD(PurpleClient, authorizeRequest);
PHP_METHOD(PurpleClient, iterate);

PHP_METHOD(PurpleAccount, __construct);
PHP_METHOD(PurpleAccount, setPassword);
PHP_METHOD(PurpleAccount, setEnabled);
PHP_METHOD(PurpleAccount, addBuddy);
PHP_METHOD(PurpleAccount, removeBuddy);
PHP_METHOD(PurpleAccount, clearSettings);
PHP_METHOD(PurpleAccount, set);
PHP_METHOD(PurpleAccount, isConnected);
PHP_METHOD(PurpleAccount, isConnecting);
PHP_METHOD(PurpleAccount, getUserName);
PHP_METHOD(PurpleAccount, getPassword);

PHP_METHOD(PurpleConnection, __construct);
PHP_METHOD(PurpleConnection, getAccount);

PHP_METHOD(PurpleConversation, __construct);
PHP_METHOD(PurpleConversation, getName);
PHP_METHOD(PurpleConversation, sendIM);
PHP_METHOD(PurpleConversation, getAccount);
PHP_METHOD(PurpleConversation, setAccount);

PHP_METHOD(PurpleBuddy, __construct);
PHP_METHOD(PurpleBuddy, getName);
PHP_METHOD(PurpleBuddy, getAlias);
PHP_METHOD(PurpleBuddy, getGroup);
PHP_METHOD(PurpleBuddy, getAccount);
PHP_METHOD(PurpleBuddy, isOnline);

PHP_METHOD(PurpleBuddyList, __construct);
PHP_METHOD(PurpleBuddyList, addBuddy);
PHP_METHOD(PurpleBuddyList, addGroup);
PHP_METHOD(PurpleBuddyList, findBuddy);
PHP_METHOD(PurpleBuddyList, load);
PHP_METHOD(PurpleBuddyList, findGroup);
PHP_METHOD(PurpleBuddyList, removeBuddy);
PHP_METHOD(PurpleBuddyList, removeGroup);

PHP_METHOD(PurpleBuddyGroup, __construct);
PHP_METHOD(PurpleBuddyGroup, getAccounts);
PHP_METHOD(PurpleBuddyGroup, getSize);
PHP_METHOD(PurpleBuddyGroup, getOnlineCount);
PHP_METHOD(PurpleBuddyGroup, getName);

ZEND_BEGIN_MODULE_GLOBALS(purple)
	zend_bool  debug_enabled;
	char *custom_user_directory;
	char *custom_plugin_path;
	char *ui_id;
	char *plugin_save_pref;
	
	/**
	 * @todo move the purple_php_client_obj into the ppos struct
	 */
	zval *purple_php_client_obj;
	
	struct php_purple_object_storage
	{
		HashTable buddy;
		HashTable group;
	} ppos; /*php purple object storage*/
ZEND_END_MODULE_GLOBALS(purple)

#define PURPLE_INI_CUSTOM_USER_DIRECTORY "/dev/null"
#define PURPLE_INI_CUSTOM_PLUGIN_PATH ""
#define PURPLE_INI_UI_ID "php"
#define PURPLE_INI_DEBUG_ENABLED "0"
#define PURPLE_INI_PLUGIN_SAVE_PREF "/purple/nullclient/plugins/saved"

#ifdef ZTS
#define PURPLE_G(v) TSRMG(purple_globals_id, zend_purple_globals *, v)
#else
#define PURPLE_G(v) (purple_globals.v)
#endif

/**
 * @todo At many places this macros is used as follows:
 * PURPLE_MK_OBJ(return_value, PurpleAccount_ce);
 * But it doesn't really affect the return_value
 * Changing the MAKE_STD_ZVAL to ZVAL_NULL does work, why?
 */
#define PURPLE_MK_OBJ(o, c) 	MAKE_STD_ZVAL(o); Z_TYPE_P(o) = IS_OBJECT; object_init_ex(o, c);

#endif	/* PHP_PURPLE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
