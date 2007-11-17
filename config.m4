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

  dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl
  dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl
  dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl
    
  SEARCH_PATH="/usr/local /usr /opt/gnome"     # you might want to change this
  SEARCH_FOR="include/glib-2.0"  # you most likely want to change this
  # search default path list
    AC_MSG_CHECKING([for glib files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        GLIB_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  
  if test -z "$GLIB_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the glib distribution])
  fi

  PHP_ADD_INCLUDE($GLIB_DIR/include/glib-2.0)
  PHP_ADD_INCLUDE($GLIB_DIR/lib/glib-2.0/include)

  LIBNAME=glib-2.0 # you may want to change this
  LIBSYMBOL=g_hash_table_new # you most likely want to change this 

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $GLIB_DIR/lib, GLIB_SHARED_LIBADD)
    AC_DEFINE(HAVE_PURPLELIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong glib lib version or lib not found])
  ],[
    -L$PURPLE_DIR/lib -lm -ldl
  ])
  
  PHP_SUBST(GLIB_SHARED_LIBADD)

  dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl
  dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl
  dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl dnl


  dnl Write more examples of tests here...

  dnl # --with-purple -> check with-path
  SEARCH_PATH="/usr/local /usr"     # you might want to change this
  SEARCH_FOR="include/libpurple"  # you most likely want to change this
  if test -r $PHP_PURPLE/$SEARCH_FOR; then # path given as parameter
    PURPLE_DIR=$PHP_PURPLE
  else # search default path list
    AC_MSG_CHECKING([for purple files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        PURPLE_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi
  
  if test -z "$PURPLE_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the purple distribution])
  fi

  dnl # --with-purple -> add include path
  PHP_ADD_INCLUDE($PURPLE_DIR/include/libpurple)

  dnl # --with-purple -> check for lib and symbol presence
  LIBNAME=purple # you may want to change this
  LIBSYMBOL=purple_core_init # you most likely want to change this 

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PURPLE_DIR/lib, PURPLE_SHARED_LIBADD)
    AC_DEFINE(HAVE_PURPLELIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong purple lib version or lib not found])
  ],[
    -L$PURPLE_DIR/lib -lm -ldl
  ])
  
  PHP_SUBST(PURPLE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(purple, [ purple.c util.c plugin.c account.c status.c \
                              signals.c connection.c core.c blist.c \
                              pounce.c ], $ext_shared)

fi
