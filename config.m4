PHP_ARG_ENABLE(raphf, whether to enable raphf support,
[  --enable-raphf           Enable resource and persistent handles factory support])

if test "$PHP_RAPHF" != "no"; then
	PHP_INSTALL_HEADERS(ext/raphf, php_raphf.h)
	PHP_NEW_EXTENSION(raphf, php_raphf.c, $ext_shared)
fi
