PHP_ARG_ENABLE(raphf, whether to enable raphf support,
[  --enable-raphf         Enable resource and persistent handles factory support])

if test "$PHP_RAPHF" != "no"; then
	PHP_RAPHF_SRCDIR=PHP_EXT_SRCDIR(raphf)
	PHP_RAPHF_BUILDDIR=PHP_EXT_BUILDDIR(raphf)

	PHP_ADD_INCLUDE($PHP_RAPHF_SRCDIR/src)
	PHP_ADD_BUILD_DIR($PHP_RAPHF_BUILDDIR/src)

	PHP_RAPHF_HEADERS=`(cd $PHP_RAPHF_SRCDIR/src && echo *.h)`
	PHP_RAPHF_SOURCES=`(cd $PHP_RAPHF_SRCDIR && echo src/*.c)`

	PHP_NEW_EXTENSION(raphf, $PHP_RAPHF_SOURCES, $ext_shared)
	PHP_INSTALL_HEADERS(ext/raphf, php_raphf.h $PHP_RAPHF_HEADERS)

	PHP_SUBST(PHP_RAPHF_HEADERS)
	PHP_SUBST(PHP_RAPHF_SOURCES)

	PHP_SUBST(PHP_RAPHF_SRCDIR)
	PHP_SUBST(PHP_RAPHF_BUILDDIR)

	PHP_ADD_MAKEFILE_FRAGMENT
fi
