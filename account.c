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
	static int account_index = 0;
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
	if(NULL != account && account_index <= ACCOUNT_LIST_LENGTH) {
		accounts_list[account_index] = account;
		account_index++;
		RETURN_LONG(account_index - 1);
	}
	
	RETURN_NULL();
}
/* }}} */

PHP_FUNCTION(purple_account_set_password)
{
	int account_index, password_len;
	char *password;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &account_index, &password, &password_len) == FAILURE) {
		RETURN_NULL();
	}
	
	if(NULL != accounts_list[account_index]) {
		purple_account_set_password(accounts_list[account_index], estrdup(password));
	}
// 	php_printf("username: %s, password: %s\n", 	purple_account_get_username(accounts_list[account_index]), 	purple_account_get_password(accounts_list[account_index]));
}

PHP_FUNCTION(purple_account_set_enabled)
{
	int account_index, ui_id_len;
	char *ui_id;
	zend_bool enabled;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lsb", &account_index, &ui_id, &ui_id_len, &enabled) == FAILURE) {
		RETURN_NULL();
	}
	
// 	enabled = (NULL == enabled) ? TRUE : enabled;

	if(NULL != accounts_list[account_index]) {
		purple_account_set_enabled(accounts_list[account_index], estrdup(ui_id), (gboolean) enabled);
// 		php_printf("protocol_id: %s\n", 	purple_account_get_protocol_id(accounts_list[account_index]));
	}
}











