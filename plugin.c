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

/* {{{ */
PHP_FUNCTION(purple_plugins_get_protocols)
{
	GList *iter;
	int i;
	
	array_init(return_value);
	
	iter = purple_plugins_get_protocols();
	for (i = 0; iter; iter = iter->next, i++) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name) {
			add_index_string(return_value, i, info->name, 1);
		}
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_plugins_add_search_path)
{
	char *plugin_path;
	int plugin_path_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &plugin_path, &plugin_path_len) == FAILURE) {
		RETURN_NULL();
	}
	
	plugin_path = !plugin_path_len ? INI_STR("purple.custom_plugin_path") : estrdup(plugin_path);
	
	purple_plugins_add_search_path(plugin_path);
}
/* }}} */











