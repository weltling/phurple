/*
   This file is part of phpurple

   Copyright (C) 2007 Anatoliy Belsky

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   In addition, as a special exception, the copyright holders of phpurple
   give you permission to combine phpurple with code included in the
   standard release of PHP under the PHP license (or modified versions of
   such code, with unchanged license). You may copy and distribute such a
   system following the terms of the GNU GPL for phpurple and the licenses
   of the other code concerned, provided that you include the source code of
   that other code when and as the GNU GPL requires distribution of source code.

   You must obey the GNU General Public License in all respects for all of the
   code used other than standard release of PHP. If you modify this file, you
   may extend this exception to your version of the file, but you are not
   obligated to do so. If you do not wish to do so, delete this exception
   statement from your version.

 */

#ifndef PHP_PURPLE_H
#define PHP_PURPLE_H

#include <glib.h>
#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "pounce.h"
#include "savedstatuses.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"
#include "version.h"

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

PHP_FUNCTION(purple_core_get_version);
PHP_FUNCTION(purple_core_init);

PHP_FUNCTION(purple_plugins_get_protocols);
PHP_FUNCTION(purple_plugins_add_search_path);
PHP_FUNCTION(purple_plugins_load_saved);

PHP_FUNCTION(purple_account_new);
PHP_FUNCTION(purple_account_set_password);
PHP_FUNCTION(purple_account_set_enabled);
PHP_FUNCTION(purple_account_is_connected);

PHP_FUNCTION(purple_util_set_user_dir);

PHP_FUNCTION(purple_savedstatus_new);
PHP_FUNCTION(purple_savedstatus_activate);

PHP_FUNCTION(purple_conversation_get_name);
PHP_FUNCTION(purple_conversation_write);
PHP_FUNCTION(purple_conversation_new);
PHP_FUNCTION(purple_conv_im_send);
PHP_FUNCTION(purple_conversation_set_account);

PHP_FUNCTION(purple_signal_connect);

PHP_FUNCTION(purple_blist_load);
PHP_FUNCTION(purple_find_buddy);
PHP_FUNCTION(purple_blist_new);

PHP_FUNCTION(purple_prefs_load);

PHP_FUNCTION(purple_pounces_load);

/*not purple functions*/
PHP_FUNCTION(purple_loop);
PHP_FUNCTION(purple_php_write_conv_function);

ZEND_BEGIN_MODULE_GLOBALS(purple)
	long  global_value;
	char *global_string;
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

#endif	/* PHP_PURPLE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
