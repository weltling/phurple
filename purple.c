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
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header,v 1.16.2.1.2.1 2007/01/01 19:32:09 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_purple.h"

#include <glib.h>
// 
// #include <string.h>
// #include <unistd.h>

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

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

static void purple_glib_io_destroy(gpointer data)
{
    g_free(data);
}

static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
    PurpleGLibIOClosure *closure = data;
    PurpleInputCondition purple_cond = 0;

    if (condition & PURPLE_GLIB_READ_COND)
        purple_cond |= PURPLE_INPUT_READ;
    if (condition & PURPLE_GLIB_WRITE_COND)
        purple_cond |= PURPLE_INPUT_WRITE;

    closure->function(closure->data, g_io_channel_unix_get_fd(source),
              purple_cond);

    return TRUE;
}

static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
                               gpointer data)
{
    PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
    GIOChannel *channel;
    GIOCondition cond = 0;

    closure->function = function;
    closure->data = data;

    if (condition & PURPLE_INPUT_READ)
        cond |= PURPLE_GLIB_READ_COND;
    if (condition & PURPLE_INPUT_WRITE)
        cond |= PURPLE_GLIB_WRITE_COND;

    channel = g_io_channel_unix_new(fd);
    closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
                          purple_glib_io_invoke, closure, purple_glib_io_destroy);

    g_io_channel_unref(channel);
    return closure->result;
}

static PurpleEventLoopUiOps glib_eventloops = 
{
    g_timeout_add,
    g_source_remove,
    glib_input_add,
    g_source_remove,
    NULL,
#if GLIB_CHECK_VERSION(2,14,0)
    g_timeout_add_seconds,
#else
    NULL,
#endif

    /* padding */
    NULL,
    NULL,
    NULL
};

/*** Conversation uiops ***/
static void
php_write_conv(PurpleConversation *conv, const char *who, const char *alias,
            const char *message, PurpleMessageFlags flags, time_t mtime)
{
    const char *name;
    if (alias && *alias)
        name = alias;
    else if (who && *who)
        name = who;
    else
        name = NULL;

    printf("(%s) %s %s: %s\n", purple_conversation_get_name(conv),
            purple_utf8_strftime("(%H:%M:%S)", localtime(&mtime)),
            name, message);
}

static PurpleConversationUiOps php_conv_uiops = 
{
    NULL,                      /* create_conversation  */
    NULL,                      /* destroy_conversation */
    NULL,                      /* write_chat           */
    NULL,                      /* write_im             */
    php_write_conv,           /* write_conv           */
    NULL,                      /* chat_add_users       */
    NULL,                      /* chat_rename_user     */
    NULL,                      /* chat_remove_users    */
    NULL,                      /* chat_update_user     */
    NULL,                      /* present              */
    NULL,                      /* has_focus            */
    NULL,                      /* custom_smiley_add    */
    NULL,                      /* custom_smiley_write  */
    NULL,                      /* custom_smiley_close  */
    NULL,                      /* send_confirm         */
    NULL,
    NULL,
    NULL,
    NULL
};

static void
php_ui_init()
{
    purple_conversations_set_ui_ops(&php_conv_uiops);
}

static PurpleCoreUiOps php_core_uiops = 
{
    NULL,
    NULL,
    php_ui_init,
    NULL,

    /* padding */
    NULL,
    NULL,
    NULL,
    NULL
};

ZEND_DECLARE_MODULE_GLOBALS(purple);

/* True global resources - no need for thread safety here */
static int le_purple;

/* {{{ purple_functions[]
 *
 * Every user visible function must have an entry in purple_functions[].
 */
zend_function_entry purple_functions[] = {
	PHP_FE(confirm_purple_compiled,	NULL)		/* For testing, remove later. */
    PHP_FE(purple_core_get_version, NULL)
    PHP_FE(purple_plugins_get_protocols, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in purple_functions[] */
};
/* }}} */

/* {{{ purple_module_entry
 */
zend_module_entry purple_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"purple",
	purple_functions,
	PHP_MINIT(purple),
	PHP_MSHUTDOWN(purple),
	PHP_RINIT(purple),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(purple),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(purple),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PURPLE
ZEND_GET_MODULE(purple)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
//     STD_PHP_INI_ENTRY("purple.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_purple_globals, purple_globals)
//     STD_PHP_INI_ENTRY("purple.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.custom_user_directory", "/dev/null", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.custom_plugin_path", "", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.ui_id", "php", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.debug_enabled", "1", PHP_INI_ALL, OnUpdateString, global_value, zend_purple_globals, purple_globals)
    STD_PHP_INI_ENTRY("purple.plugin_save_pref", "/dev/null", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
PHP_INI_END()

/* }}} */

/* {{{ php_purple_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_purple_init_globals(zend_purple_globals *purple_globals)
{
	purple_globals->global_value = 0;
	purple_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(purple)
{
	REGISTER_INI_ENTRIES();
    
    /* purple initialization stuff
    */
    purple_util_set_user_dir(INI_STR("purple.custom_user_directory"));
    purple_debug_set_enabled(INI_INT("purple.debug_enabled"));
    purple_core_set_ui_ops(&php_core_uiops);
    purple_eventloop_set_ui_ops(&glib_eventloops);
    purple_plugins_add_search_path(INI_STR("purple.custom_plugin_path"));
    
    if (!purple_core_init(INI_STR("purple.ui_id"))) {
        /* Initializing the core failed. Terminate. */
        fprintf(stderr,
                "libpurple initialization failed. Dumping core.\n"
                "Please report this!\n");
        abort();
    }
    
    /* Create and load the buddylist. */
//     purple_set_blist(purple_blist_new());
    purple_blist_load();

    /* Load the preferences. */
    purple_prefs_load();

    /* Load the desired plugins. The client should save the list of loaded plugins in
     * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
    purple_plugins_load_saved(INI_STR("purple.plugin_save_pref"));

    /* Load the pounces. */
    purple_pounces_load();
    
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(purple)
{
	UNREGISTER_INI_ENTRIES();
	
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(purple)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(purple)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(purple)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "purple support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_purple_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_purple_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "purple", arg);
	RETURN_STRINGL(strg, len, 0);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
