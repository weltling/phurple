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
PHP_FUNCTION(purple_account_new)
{
	char *username, *protocol_name, *protocol_id;
	int username_len, protocol_name_len;
	GList *iter;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &username, &username_len, &protocol_name, &protocol_name_len) == FAILURE) {
		RETURN_NULL();
	}
	
	iter = purple_plugins_get_protocols();
	for (; iter; iter = iter->next) {
		PurplePlugin *plugin = iter->data;
		PurplePluginInfo *info = plugin->info;
		if (info && info->name && 0 == strcmp(info->name, protocol_name)) {
			protocol_id = estrdup(info->id);
		}
	}
//     php_printf("%s %s\n", username, protocol_id);
	account = purple_account_new(estrdup(username), estrdup(protocol_id));
	if(NULL != account) {
		accounts_list = g_list_append(accounts_list, account);
		RETURN_LONG((int)g_list_position(accounts_list, g_list_last(accounts_list)));
	}
	
	RETURN_NULL();
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_account_set_password)
{
	int account_index, password_len;
	char *password;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &account_index, &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}
	
	account = g_list_nth_data (accounts_list, (guint)account_index);
	if(NULL != account) {
		purple_account_set_password(account, estrdup(password));
	}
}
/* }}} */


/* {{{ */
PHP_FUNCTION(purple_account_set_enabled)
{
	int account_index, ui_id_len;
	char *ui_id;
	zend_bool enabled;
	PurpleAccount *account = NULL;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lsb", &account_index, &ui_id, &ui_id_len, &enabled) == FAILURE) {
		RETURN_NULL();
	}

	account = g_list_nth_data (accounts_list, (guint)account_index);
	if(NULL != account) {
		purple_account_set_enabled(account, estrdup(ui_id), (gboolean) enabled);
	}
}
/* }}} */










