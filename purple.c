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

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#include <sys/wait.h>
#endif

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

static void php_ui_init();
static zval *purple_php_string_zval(const char *str);
static zval *purple_php_long_zval(long l);
static void purple_glib_io_destroy(gpointer data);
static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data);
static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							gpointer data);
static void
purple_php_write_conv_function(PurpleConversation *conv, const char *who, const char *alias,
							   const char *message, PurpleMessageFlags flags, time_t mtime);
#ifdef HAVE_SIGNAL_H
static void sighandler(int sig);
static void clean_pid();
#endif

static zval *purple_php_write_conv_function_name;

#ifdef HAVE_SIGNAL_H
static char *segfault_message;

static int catch_sig_list[] = {
	SIGSEGV,
	SIGHUP,
	SIGINT,
	SIGTERM,
	SIGQUIT,
	SIGCHLD,
	SIGALRM,
	-1
};

static int ignore_sig_list[] = {
	SIGPIPE,
	-1
};
#endif

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
static PurpleConversationUiOps php_conv_uiops =
{
	NULL,                      /* create_conversation  */
	NULL,                      /* destroy_conversation */
	NULL,            /* write_chat           */
	NULL,              /* write_im             */
	purple_php_write_conv_function,            /* write_conv           */
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
	PHP_FE(purple_core_get_version, NULL)
	PHP_FE(purple_core_init, NULL)
	
	PHP_FE(purple_plugins_get_protocols, NULL)
	PHP_FE(purple_plugins_add_search_path, NULL)
	PHP_FE(purple_plugins_load_saved, NULL)
	
	PHP_FE(purple_account_new, NULL)
	PHP_FE(purple_account_set_password, NULL)
	PHP_FE(purple_account_set_enabled, NULL)
	
	PHP_FE(purple_util_set_user_dir, NULL)
	
	PHP_FE(purple_savedstatus_new, NULL)
	PHP_FE(purple_savedstatus_activate, NULL)

	PHP_FE(purple_conversation_get_name, NULL)
			
	PHP_FE(purple_signal_connect, NULL)
			
	PHP_FE(purple_blist_load, NULL)
			
	PHP_FE(purple_prefs_load, NULL)
			
	PHP_FE(purple_pounces_load, NULL)
			
	/*not purple functions*/
	PHP_FE(purple_loop, NULL)
	PHP_FE(purple_php_write_conv_function, NULL)
		
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
	STD_PHP_INI_ENTRY("purple.plugin_save_pref", "/purple/nullclient/plugins/saved", PHP_INI_ALL, OnUpdateString, global_string, zend_purple_globals, purple_globals)
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
//     php_printf("%d\n", PURPLE_STATUS_AVAILABLE);
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_UNSET",         PURPLE_STATUS_UNSET,         CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_OFFLINE",       PURPLE_STATUS_OFFLINE,       CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_AVAILABLE",     PURPLE_STATUS_AVAILABLE,     CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_UNAVAILABLE",   PURPLE_STATUS_UNAVAILABLE,   CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_INVISIBLE",     PURPLE_STATUS_INVISIBLE,     CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_AWAY",          PURPLE_STATUS_AWAY,          CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_EXTENDED_AWAY", PURPLE_STATUS_EXTENDED_AWAY, CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT("PURPLE_STATUS_MOBILE",        PURPLE_STATUS_MOBILE,        CONST_CS | CONST_PERSISTENT );

#ifdef HAVE_SIGNAL_H
	int sig_indx;	/* for setting up signal catching */
	sigset_t sigset;
	RETSIGTYPE (*prev_sig_disp)(int);
	char errmsg[BUFSIZ];

	if (sigemptyset(&sigset)) {
		snprintf(errmsg, BUFSIZ, "Warning: couldn't initialise empty signal set");
		perror(errmsg);
	}
	for(sig_indx = 0; catch_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(catch_sig_list[sig_indx], sighandler)) == SIG_ERR) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't set signal %d for catching",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
		if(sigaddset(&sigset, catch_sig_list[sig_indx])) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't include signal %d for unblocking",
				catch_sig_list[sig_indx]);
			perror(errmsg);
		}
	}
	for(sig_indx = 0; ignore_sig_list[sig_indx] != -1; ++sig_indx) {
		if((prev_sig_disp = signal(ignore_sig_list[sig_indx], SIG_IGN)) == SIG_ERR) {
			snprintf(errmsg, BUFSIZ, "Warning: couldn't set signal %d to ignore",
				ignore_sig_list[sig_indx]);
			perror(errmsg);
		}
	}

	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL)) {
		snprintf(errmsg, BUFSIZ, "Warning: couldn't unblock signals");
		perror(errmsg);
	}
#endif
	
    /* purple initialization stuff
    */
	purple_util_set_user_dir(INI_STR("purple.custom_user_directory"));
    purple_debug_set_enabled(INI_INT("purple.debug_enabled"));
	purple_core_set_ui_ops(&php_core_uiops);
	purple_eventloop_set_ui_ops(&glib_eventloops);
	purple_plugins_add_search_path(INI_STR("purple.custom_plugin_path"));

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


/* {{{ */
PHP_FUNCTION(purple_loop)
{
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_prefs_load)
{
	RETURN_BOOL((long)purple_prefs_load());
}
/* }}} */

/* {{{ */
PHP_FUNCTION(purple_php_write_conv_function)
{
	zval *arg;
	char *name;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &arg)) {
		RETURN_FALSE;
	}

	if (!zend_is_callable(arg, 0, &name)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s is not callable", name);
		efree(name);
		RETURN_FALSE;
	}

	purple_php_write_conv_function_name = purple_php_string_zval(name);
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_conversation_get_name)
{
	int conversation_index;
	PurpleConversation *conversation = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &conversation_index) == FAILURE) {
		RETURN_NULL();
	}

	conversation = g_list_nth_data (conversations_list, (guint)conversation_index);
// php_printf("conversation name: %s\n", purple_conversation_get_name(conversation));
	if(NULL != conversation) {
		RETURN_STRING(purple_conversation_get_name(conversation), 1);
	}

	RETURN_NULL();
}
/* }}} */



/*
**
** Helper functions
**
*/

static void
php_ui_init()
{
	purple_conversations_set_ui_ops(&php_conv_uiops);
}

#ifdef HAVE_SIGNAL_H
static void
clean_pid()
{
	int status;
	pid_t pid;

	do {
		pid = waitpid(-1, &status, WNOHANG);
	} while (pid != 0 && pid != (pid_t)-1);

	if ((pid == (pid_t) - 1) && (errno != ECHILD)) {
		char errmsg[BUFSIZ];
		snprintf(errmsg, BUFSIZ, "Warning: waitpid() returned %d", pid);
		perror(errmsg);
	}

	/* Restore signal catching */
	signal(SIGALRM, sighandler);
}

static void
sighandler(int sig)
{
	switch (sig) {
	case SIGHUP:
		purple_debug_warning("sighandler", "Caught signal %d\n", sig);
		purple_connections_disconnect_all();
		break;
	case SIGSEGV:
		fprintf(stderr, "%s", segfault_message);
		abort();
		break;
	case SIGCHLD:
		/* Restore signal catching */
		signal(SIGCHLD, sighandler);
		alarm(1);
		break;
	case SIGALRM:
		clean_pid();
		break;
	default:
		purple_debug_warning("sighandler", "Caught signal %d\n", sig);
		purple_connections_disconnect_all();

		purple_plugins_unload_all();

		exit(0);
	}
}
#endif


/* {{{ just took this two functions from the readline extension */
static zval *purple_php_string_zval(const char *str)
{
	zval *ret;
	int len;
	
	MAKE_STD_ZVAL(ret);
	
	if (str) {
		len = strlen(str);
		ZVAL_STRINGL(ret, (char*)str, len, 1);
	} else {
		ZVAL_NULL(ret);
	}

	return ret;
}

static zval *purple_php_long_zval(long l)
{
	zval *ret;
	MAKE_STD_ZVAL(ret);

	Z_TYPE_P(ret) = IS_LONG;
	Z_LVAL_P(ret) = l;
	return ret;
}
/* }}} */

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


/* {{{ */
static void
purple_php_write_conv_function(PurpleConversation *conv, const char *who, const char *alias,
            const char *message, PurpleMessageFlags flags, time_t mtime)
{
	const int PARAMS_COUNT = 5;
	zval *params[PARAMS_COUNT];
	zval *retval;
	
	TSRMLS_FETCH();

	if(NULL == g_list_find(conversations_list, conv)) {
		conversations_list = g_list_append(conversations_list, conv);
	}

	params[0] = purple_php_long_zval((long)g_list_position(conversations_list, g_list_last(conversations_list)));
	params[1] = purple_php_long_zval((long)mtime);
	params[2] = purple_php_string_zval(who);
	params[3] = purple_php_string_zval(alias);
	params[4] = purple_php_string_zval(message);

	if(call_user_function(	CG(function_table),
	   						NULL,
							purple_php_write_conv_function_name,
							retval,
							PARAMS_COUNT,
							params TSRMLS_CC
						 ) != SUCCESS) {
		zend_error(E_ERROR, "Function call failed");
	}

	zval_dtor(retval);
}
/* }}} */

/*
static void php_write_chat(PurpleConversation *conv, const char *who, const char *message,
						   PurpleMessageFlags flags, time_t mtime)
{
	php_printf("%s + write_chat\n", message);
}

static void php_write_im(PurpleConversation *conv, const char *who, const char *message,
						 PurpleMessageFlags flags, time_t mtime)
{
	php_printf("%s + write_im\n", message);
	
// 	purple_conv_im_send(purple_conversation_get_im_data(conv), "answer: you are an asshole ;) !!! \n");
// 	purple_conv_im_write(purple_conversation_get_im_data(conv), who, "answer: you are an asshole ;) !!! \n", flags, mtime);
// 	common_send(conv, g_strdup("answer: you are an asshole ;) !!! \n"), flags);
}*/






/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
