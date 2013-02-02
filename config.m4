PHP_ARG_ENABLE(raphf, whether to enable raphf support,
[  --enable-raphf           Enable raphf support])

if test "$PHP_RAPHF" != "no"; then
	dnl PHP_SUBST(RAPHF_SHARED_LIBADD)
	PHP_INSTALL_HEADERS(ext/raphf, php_raphf.h)
	PHP_NEW_EXTENSION(raphf, php_raphf.c, $ext_shared)
fi
