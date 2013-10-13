dnl config.m4 for extension phurple

PHP_ARG_WITH(phurple, for phurple support,
[  --with-purple             Include phurple support])

if test "$PHP_PHURPLE" != "no"; then

	CFLAGS="$CFLAGS -g3"

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
		AC_MSG_ERROR([wrong glib lib version or glib not found])
	],[
		-L$GLIB_DIR/lib -lglib-2.0
	])
  
	PHP_SUBST(GLIB_SHARED_LIBADD)

	dnl end check for glib

	dnl check for libpurple

	SEARCH_PATH="/usr/local /usr"
	SEARCH_FOR="include/libpurple"
	if test -r $PHP_PHURPLE/$SEARCH_FOR; then # path given as parameter
		PHURPLE_DIR=$PHP_PHURPLE
	else # search default path list
		AC_MSG_CHECKING([for purple files in default path])
		for i in $SEARCH_PATH ; do
			if test -r $i/$SEARCH_FOR; then
				PHURPLE_DIR=$i
				AC_MSG_RESULT(found in $i)
				break
			fi
		done
	fi
  
	if test -z "$PHURPLE_DIR"; then
		AC_MSG_RESULT([not found])
		AC_MSG_ERROR([Please reinstall the phurple distribution])
	fi

	LIBNAME=purple
	LIBSYMBOL=purple_core_init

	PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
	[
		PHP_ADD_INCLUDE($PHURPLE_DIR/include/libpurple)
		PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PHURPLE_DIR/lib, PHURPLE_SHARED_LIBADD)
		AC_DEFINE(HAVE_PHURPLELIB,1,[ ])
	],[
		AC_MSG_ERROR([wrong purple lib version or lib not found])
	],[
		-L$PHURPLE_DIR/lib -lpurple
	])
	
	PHP_SUBST(PHURPLE_SHARED_LIBADD)

	dnl end check for libpurple


	dnl check for pcre, weither bundled or system header should be there
	
	AC_CHECK_HEADER([pcre.h], [], AC_MSG_ERROR([pcre.h not found]))

	dnl end check for pcre

	PHP_NEW_EXTENSION(phurple, [ phurple.c client.c conversation.c account.c \
	                             connection.c buddy.c buddylist.c group.c \
								presence.c \
	                           ], $ext_shared)

fi
