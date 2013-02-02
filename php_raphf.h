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

/* $Id$ */

#ifndef PHP_RAPHF_H
#define PHP_RAPHF_H

extern zend_module_entry raphf_module_entry;
#define phpext_raphf_ptr &raphf_module_entry

#define PHP_RAPHF_VERSION "1.0.0dev"

#ifdef PHP_WIN32
#	define PHP_RAPHF_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_RAPHF_API __attribute__ ((visibility("default")))
#else
#	define PHP_RAPHF_API
#endif

#ifdef ZTS
#	include "TSRM.h"
#endif

typedef void *(*php_resource_factory_handle_ctor_t)(void *opaque, void *init_arg TSRMLS_DC);
typedef void *(*php_resource_factory_handle_copy_t)(void *opaque, void *handle TSRMLS_DC);
typedef void (*php_resource_factory_handle_dtor_t)(void *opaque, void *handle TSRMLS_DC);

typedef struct php_resource_factory_ops {
	php_resource_factory_handle_ctor_t ctor;
	php_resource_factory_handle_copy_t copy;
	php_resource_factory_handle_dtor_t dtor;
} php_resource_factory_ops_t;

typedef struct php_resource_factory {
	php_resource_factory_ops_t fops;

	void *data;
	void (*dtor)(void *data);

	unsigned refcount;
} php_resource_factory_t;

PHP_RAPHF_API php_resource_factory_t *php_resource_factory_init(php_resource_factory_t *f, php_resource_factory_ops_t *fops, void *data, void (*dtor)(void *data));
PHP_RAPHF_API unsigned php_resource_factory_addref(php_resource_factory_t *rf);
PHP_RAPHF_API void php_resource_factory_dtor(php_resource_factory_t *f);
PHP_RAPHF_API void php_resource_factory_free(php_resource_factory_t **f);

PHP_RAPHF_API void *php_resource_factory_handle_ctor(php_resource_factory_t *f, void *init_arg TSRMLS_DC);
PHP_RAPHF_API void *php_resource_factory_handle_copy(php_resource_factory_t *f, void *handle TSRMLS_DC);
PHP_RAPHF_API void php_resource_factory_handle_dtor(php_resource_factory_t *f, void *handle TSRMLS_DC);

typedef struct php_persistent_handle_factory {
	void *provider;

	struct {
		char *str;
		size_t len;
	} ident;

	unsigned free_on_abandon:1;
} php_persistent_handle_factory_t;

struct php_persistent_handle_globals {
	ulong limit;
	struct {
		ulong h;
		char *s;
		size_t l;
	} ident;
};

PHP_RAPHF_API int /* SUCCESS|FAILURE */ php_persistent_handle_provide(const char *name_str, size_t name_len, php_resource_factory_ops_t *fops, void *data, void (*dtor)(void *));
PHP_RAPHF_API php_persistent_handle_factory_t *php_persistent_handle_concede(php_persistent_handle_factory_t *a, const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC);
PHP_RAPHF_API void php_persistent_handle_abandon(php_persistent_handle_factory_t *a);
PHP_RAPHF_API void *php_persistent_handle_acquire(php_persistent_handle_factory_t *a, void *init_arg TSRMLS_DC);
PHP_RAPHF_API void php_persistent_handle_release(php_persistent_handle_factory_t *a, void *handle TSRMLS_DC);
PHP_RAPHF_API void *php_persistent_handle_accrete(php_persistent_handle_factory_t *a, void *handle TSRMLS_DC);

PHP_RAPHF_API php_resource_factory_ops_t *php_persistent_handle_get_resource_factory_ops(void);

PHP_RAPHF_API void php_persistent_handle_cleanup(const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC);
PHP_RAPHF_API HashTable *php_persistent_handle_statall(HashTable *ht TSRMLS_DC);

ZEND_BEGIN_MODULE_GLOBALS(raphf)
	struct php_persistent_handle_globals persistent_handle;
ZEND_END_MODULE_GLOBALS(raphf)

#ifdef ZTS
#	define PHP_RAPHF_G ((zend_raphf_globals *) (*((void ***) tsrm_ls))[TSRM_UNSHUFFLE_RSRC_ID(raphf_globals_id)])
#else
#	define PHP_RAPHF_G (&raphf_globals)
#endif

#endif	/* PHP_RAPHF_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
