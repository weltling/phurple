/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2014 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Anatol Belski <ab@php.net>                                   |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_SIZE_INT_COMPAT_H
#define PHP_SIZE_INT_COMPAT_H

/* XXX change the check accordingly to the vote results */
#define PHP_NEED_STRSIZE_COMPAT (PHP_MAJOR_VERSION < 5) || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 6) || defined(PHP_FORCE_STRSIZE_COMPAT)

#if PHP_NEED_STRSIZE_COMPAT

typedef long zend_int_t;
typedef unsigned long zend_uint_t;
typedef long zend_off_t;
typedef int zend_size_t;
typedef struct stat zend_stat_t;
# define ZEND_INT_MAX LONG_MAX
# define ZEND_INT_MIN LONG_MIN
# define ZEND_UINT_MAX ULONG_MAX
# define ZEND_SIZE_MAX SIZE_MAX
# define Z_I(i) i
# define Z_UI(i) i
# define SIZEOF_ZEND_INT SIZEOF_LONG
# define ZEND_STRTOL(s0, s1, base) strtol((s0), (s1), (base))
# define ZEND_STRTOUL(s0, s1, base) strtoul((s0), (s1), (base))
# ifdef PHP_WIN32
#  define ZEND_ITOA(i, s, len) _ltoa_s((i), (s), (len), 10)
#  define ZEND_ATOI(i, s) i = atol((s))
# else
#  define ZEND_ITOA(i, s, len) \
	do { \
		int st = snprintf((s), (len), "%ld", (i)); \
		(s)[st] = '\0'; \
 	} while (0)
#  define ZEND_ATOI(i, s) (i) = atol((s))
# endif
# define ZEND_INT_FMT "%ld"
# define ZEND_UINT_FMT "%lu"
# define ZEND_INT_FMT_SPEC "ld"
# define ZEND_UINT_FMT_SPEC "lu"
# define ZEND_STRTOL_PTR strtol
# define ZEND_STRTOUL_PTR strtoul
# define ZEND_ABS abs
# define zend_fseek fseek
# define zend_ftell ftell
# define zend_lseek lseek
# define zend_fstat fstat
# define zend_stat stat
# define php_fstat fstat
# define php_stat_fn stat

#define php_size_t zend_size_t
#define php_int_t zend_int_t
#define php_uint_t zend_uint_t
#define php_stat_t zend_stat_t
#define PHP_INT_MAX ZEND_INT_MAX
#define PHP_INT_MIN ZEND_INT_MIN
#define PHP_UINT_MAX ZEND_UINT_MAX
#define PHP_SIZE_MAX ZEND_SIZE_MAX

#endif

#endif /* PHP_SIZE_INT_COMPAT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
