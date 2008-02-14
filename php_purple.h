/*
Copyright (c) 2007-2008, Anatoliy Belsky
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Source code and binaries may NOT be SOLD in any manner without the explicit written consent of the copyright holder.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

PHP_METHOD(Client, __construct);
PHP_METHOD(Client, getInstance);
PHP_METHOD(Client, initInternal);
PHP_METHOD(Client, getCoreVersion);
PHP_METHOD(Client, connectToSignal);
PHP_METHOD(Client, writeConv);
PHP_METHOD(Client, writeIM);
PHP_METHOD(Client, onSignedOn);
PHP_METHOD(Client, runLoop);
PHP_METHOD(Client, addAccount);
PHP_METHOD(Client, getProtocols);
PHP_METHOD(Client, setUserDir);
PHP_METHOD(Client, loopCallback);
PHP_METHOD(Client, loopTickCallback);

PHP_METHOD(Account, __construct);
PHP_METHOD(Account, setPassword);
PHP_METHOD(Account, setEnabled);
PHP_METHOD(Account, addBuddy);
PHP_METHOD(Account, removeBuddy);

PHP_METHOD(Connection, __construct);
PHP_METHOD(Connection, getAccount);

PHP_METHOD(Conversation, __construct);
PHP_METHOD(Conversation, getName);
PHP_METHOD(Conversation, sendIM);
PHP_METHOD(Conversation, getAccount);

PHP_METHOD(Buddy, __construct);
PHP_METHOD(Buddy, getName);
PHP_METHOD(Buddy, getAlias);
PHP_METHOD(Buddy, getGroup);
PHP_METHOD(Buddy, getAccount);
PHP_METHOD(Buddy, updateStatus);
PHP_METHOD(Buddy, isOnline);

PHP_METHOD(BuddyList, __construct);
PHP_METHOD(BuddyList, addBuddy);
PHP_METHOD(BuddyList, addGroup);
PHP_METHOD(BuddyList, getGroups);
PHP_METHOD(BuddyList, getBuddies);
PHP_METHOD(BuddyList, findBuddy);
PHP_METHOD(BuddyList, load);

PHP_METHOD(BuddyGroup, __construct);
PHP_METHOD(BuddyGroup, getAccounts);
PHP_METHOD(BuddyGroup, getSize);
PHP_METHOD(BuddyGroup, getOnlineCount);
PHP_METHOD(BuddyGroup, getName);

ZEND_BEGIN_MODULE_GLOBALS(purple)
	long  debug_enabled;
	char *custom_user_directory;
	char *custom_plugin_path;
	char *ui_id;
	char *plugin_save_pref;
	zval *purple_php_client_obj;
	struct php_purple_object_storage
	{
		HashTable buddy;
		HashTable group;
	} ppos; /*php purple object storage*/
ZEND_END_MODULE_GLOBALS(purple)


/* In every utility function you add that needs to use variables 
   in php_purple_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as PURPLE_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define PURPLE_G(v) TSRMG(purple_globals_id, zend_purple_globals *, v)
#else
#define PURPLE_G(v) (purple_globals.v)
#endif

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
