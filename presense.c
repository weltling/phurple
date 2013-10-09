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

extern zval *
php_create_account_obj_zval(PurpleAccount *paccount TSRMLS_DC);

#if PHURPLE_INTERNAL_DEBUG
extern void phurple_dump_zval(zval *var);
#endif

void
php_presence_obj_destroy(void *obj TSRMLS_DC)
{
	struct ze_presence_obj *zpo = (struct ze_presence_obj *)obj;

	zend_object_std_dtor(&zpo->zo TSRMLS_CC);

	/*if (zco->ppresence) {
		purple_presence_destroy(zco->ppresence);
	}*/

	efree(zpo);
}

zend_object_value
php_presence_obj_init(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ret;
	struct ze_presence_obj *zpo;
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4
	zval *tmp;
#endif

	zpo = (struct ze_presence_obj *) emalloc(sizeof(struct ze_presence_obj));
	memset(&zpo->zo, 0, sizeof(zend_object));

	zend_object_std_init(&zpo->zo, ce TSRMLS_CC);
#if PHP_MAJOR_VERSION== 5 && PHP_MINOR_VERSION < 4
	zend_hash_copy(zpo->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref,
					(void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&zpo->zo, ce);
#endif

	zpo->ppresence = NULL;

	ret.handle = zend_objects_store_put(zpo, NULL,
								(zend_objects_free_object_storage_t) php_presence_obj_destroy,
								NULL TSRMLS_CC);

	ret.handlers = &default_phurple_obj_handlers;

	return ret;
}

/*
**
**
** Phurple presence methods
**
*/

/* {{{ proto object PhurplePresence::__construct()
	constructor*/
PHP_METHOD(PhurplePresence, __construct)
{
	/* XXX to implement here */	
}
/* }}} */

zval *
php_create_presence_obj_zval(PurplePresence *ppresence TSRMLS_DC)
{
	zval *ret;
	struct ze_presence_obj *zpo;

	ALLOC_ZVAL(ret);
	object_init_ex(ret, PhurplePresence_ce);
	INIT_PZVAL(ret);

	zpo = (struct ze_presence_obj *) zend_object_store_get_object(ret TSRMLS_CC);
	zpo->ppresence = ppresence;

	return ret;
}

		// setting current account status seems to have nothing to do with presence
		//purple_presence_set_status_active(ppresence, "available", 1);
		//purple_presence_switch_status(ppresence, purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE));
/*
**
**
** End phurple presence methods
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
