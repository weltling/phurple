/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Anatoliy Belsky                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

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

#define ACCOUNT_LIST_LENGTH 10

typedef struct _PurpleGLibIOClosure PurpleGLibIOClosure;

// GList *protocols_list;

static PurpleAccount* accounts_list[ACCOUNT_LIST_LENGTH];

static PurpleSavedStatus* saved_status;

// void* connections_handle;

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

PHP_FUNCTION(purple_util_set_user_dir);

PHP_FUNCTION(purple_savedstatus_new);
PHP_FUNCTION(purple_savedstatus_activate);

PHP_FUNCTION(purple_signal_connect);

PHP_FUNCTION(purple_blist_load);

PHP_FUNCTION(purple_prefs_load);

PHP_FUNCTION(purple_pounces_load);

/*not purple functions*/
PHP_FUNCTION(purple_loop);


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

/* Define some purple settings
*/
// #define PURPLE_CUSTOM_USER_DIRECTORY    "/dev/null"
// #define PURPLE_CUSTOM_PLUGIN_PATH       ""
// #define PURPLE_UI_ID                    "php"
// #define PURPLE_DEBUG_ENABLED            TRUE

struct _PurpleGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
};
        
#endif	/* PHP_PURPLE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
