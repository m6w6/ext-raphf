/*
    +--------------------------------------------------------------------+
    | PECL :: raphf                                                      |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2013, Michael Wallner <mike@php.net>                 |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_RAPHF_H
#define PHP_RAPHF_H

extern zend_module_entry raphf_module_entry;
#define phpext_raphf_ptr &raphf_module_entry

#define PHP_RAPHF_VERSION "2.0.0"

#ifdef PHP_WIN32
#	define PHP_RAPHF_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_RAPHF_API extern __attribute__ ((visibility("default")))
#else
#	define PHP_RAPHF_API extern
#endif

#ifdef ZTS
#	include "TSRM.h"
#endif

#include "php_raphf_api.h"

#endif	/* PHP_RAPHF_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
