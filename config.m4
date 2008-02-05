dnl $Id$
dnl config.m4 for extension purple

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(purple, for purple support,
dnl Make sure that the comment is aligned:
[  --with-purple             Include purple support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(purple, whether to enable purple support,
dnl Make sure that the comment is aligned:
[  --enable-purple           Enable purple support])

if test "$PHP_PURPLE" != "no"; then

	dnl check for glib
	
	SEARCH_PATH="/usr/local /usr /opt/gnome"
	SEARCH_FOR="include/glib-2.0"
	# search default path list
	AC_MSG_CHECKING([for glib files in default path])
	for i in $SEARCH_PATH ; do
		if test -r $i/$SEARCH_FOR; then
			GLIB_DIR=$i
			AC_MSG_RESULT(found in $i)
			break
		fi
	done
	
	if test -z "$GLIB_DIR"; then
		AC_MSG_RESULT([not found])
		AC_MSG_ERROR([Please reinstall the glib distribution])
	fi

	LIBNAME=glib-2.0
	LIBSYMBOL=g_hash_table_new

	PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
	[
		PHP_ADD_INCLUDE($GLIB_DIR/include/glib-2.0)
		PHP_ADD_INCLUDE($GLIB_DIR/lib/glib-2.0/include)
		PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $GLIB_DIR/lib, GLIB_SHARED_LIBADD)
		AC_DEFINE(HAVE_GLIBLIB,1,[ ])
	],[
		AC_MSG_ERROR([wrong glib lib version or lib not found])
	],[
		-L$GLIB_DIR/lib -lglib-2.0
	])
  
	PHP_SUBST(GLIB_SHARED_LIBADD)

	dnl end check for glib

	dnl check for libpurple

	SEARCH_PATH="/usr/local /usr"
	SEARCH_FOR="include/libpurple"
	if test -r $PHP_PURPLE/$SEARCH_FOR; then # path given as parameter
		PURPLE_DIR=$PHP_PURPLE
	else # search default path list
		AC_MSG_CHECKING([for purple files in default path])
		for i in $SEARCH_PATH ; do
			if test -r $i/$SEARCH_FOR; then
				PURPLE_DIR=$i
				AC_MSG_RESULT(found in $i)
				break
			fi
		done
	fi
  
	if test -z "$PURPLE_DIR"; then
		AC_MSG_RESULT([not found])
		AC_MSG_ERROR([Please reinstall the purple distribution])
	fi

	LIBNAME=purple
	LIBSYMBOL=purple_core_init

	PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
	[
		PHP_ADD_INCLUDE($PURPLE_DIR/include/libpurple)
		PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PURPLE_DIR/lib, PURPLE_SHARED_LIBADD)
		AC_DEFINE(HAVE_PURPLELIB,1,[ ])
	],[
		AC_MSG_ERROR([wrong purple lib version or lib not found])
	],[
		-L$PURPLE_DIR/lib -lpurple
	])
	
	PHP_SUBST(PURPLE_SHARED_LIBADD)

	dnl end check for libpurple

  PHP_NEW_EXTENSION(purple, [ purple.c ], $ext_shared)

fi
