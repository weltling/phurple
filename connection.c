/**
 * Copyright (c) 2007-2011, Anatol Belski <ab@php.net>
 *
 * This file is part of Phurple.
 *
 * Phurple is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Phurple is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Phurple.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>

#include "php_phurple.h"

#include <glib.h>

#include <string.h>
#include <ctype.h>

#include <purple.h>

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

/*
**
**
** Phurple connection methods
**
*/

/* {{{ proto object PhurpleConnection::__construct()
	constructor*/
PHP_METHOD(PhurpleConnection, __construct)
{
	
}
/* }}} */


/* {{{ proto PhurpleAccount PhurpleConnection::getAccount()
		Returns the connection's account*/
PHP_METHOD(PhurpleConnection, getAccount)
{
	PurpleConnection *conn = NULL;
	PurpleAccount *acc = NULL;

	conn = g_list_nth_data (purple_connections_get_all(), (guint)Z_LVAL_P(zend_read_property(PhurpleConnection_ce, getThis(), "index", sizeof("index")-1, 0 TSRMLS_CC)));
	if(NULL != conn) {
		acc = purple_connection_get_account(conn);
		if(NULL != acc) {
			zval *ret;
			struct ze_account_obj *zao;

			MAKE_STD_ZVAL(ret);
			object_init_ex(ret, PhurpleAccount_ce);

			zao = (struct ze_account_obj *) zend_object_store_get_object(ret TSRMLS_CC);

			zao->paccount = acc;

			*return_value = *ret;

			return;
		}
	}

	RETURN_NULL();
}
/* }}} */


/*
**
**
** End phurple connection methods
**
*/
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
