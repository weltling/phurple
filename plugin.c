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



/* {{{ */
PHP_FUNCTION(purple_plugins_load_saved)
{
	char* key;
	int key_len;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &key, &key_len) == FAILURE) {
		RETURN_NULL();
	}
	
	key = !key_len ? INI_STR("purple.plugin_save_pref") : estrdup(key);
	
	purple_plugins_load_saved(key);
}

/* }}} */




