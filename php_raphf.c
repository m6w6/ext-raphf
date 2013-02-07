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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_raphf.h"

ZEND_DECLARE_MODULE_GLOBALS(raphf)

typedef int STATUS;

#ifndef PHP_RAPHF_DEBUG_PHANDLES
#	define PHP_RAPHF_DEBUG_PHANDLES 0
#endif
#if PHP_RAPHF_DEBUG_PHANDLES
#	undef inline
#	define inline
#endif

static HashTable php_persistent_handle_hash;

#ifdef ZTS
#	define LOCK() tsrm_mutex_lock(php_persistent_handle_lock)
#	define UNLOCK() tsrm_mutex_unlock(php_persistent_handle_lock)
static MUTEX_T php_persistent_handle_lock;
#else
#	define LOCK()
#	define UNLOCK()
#endif


PHP_RAPHF_API php_resource_factory_t *php_resource_factory_init(php_resource_factory_t *f, php_resource_factory_ops_t *fops, void *data, void (*dtor)(void *data))
{
	if (!f) {
		f = emalloc(sizeof(*f));
	}
	memset(f, 0, sizeof(*f));

	memcpy(&f->fops, fops, sizeof(*fops));

	f->data = data;
	f->dtor = dtor;

	f->refcount = 1;

	return f;
}

PHP_RAPHF_API unsigned php_resource_factory_addref(php_resource_factory_t *rf)
{
	return ++rf->refcount;
}

PHP_RAPHF_API void php_resource_factory_dtor(php_resource_factory_t *f)
{
	--f->refcount;

	if (!f->refcount) {
		if (f->dtor) {
			f->dtor(f->data);
		}
	}
}

PHP_RAPHF_API void php_resource_factory_free(php_resource_factory_t **f)
{
	if (*f) {
		php_resource_factory_dtor(*f);
		if (!(*f)->refcount) {
			efree(*f);
			*f = NULL;
		}
	}
}

PHP_RAPHF_API void *php_resource_factory_handle_ctor(php_resource_factory_t *f, void *init_arg TSRMLS_DC)
{
	if (f->fops.ctor) {
		return f->fops.ctor(f->data, init_arg TSRMLS_CC);
	}
	return NULL;
}

PHP_RAPHF_API void *php_resource_factory_handle_copy(php_resource_factory_t *f, void *handle TSRMLS_DC)
{
	if (f->fops.copy) {
		return f->fops.copy(f->data, handle TSRMLS_CC);
	}
	return NULL;
}

PHP_RAPHF_API void php_resource_factory_handle_dtor(php_resource_factory_t *f, void *handle TSRMLS_DC)
{
	if (f->fops.dtor) {
		f->fops.dtor(f->data, handle TSRMLS_CC);
	}
}

static inline php_persistent_handle_list_t *php_persistent_handle_list_init(php_persistent_handle_list_t *list)
{
	int free_list;

	if ((free_list = !list)) {
		list = pemalloc(sizeof(php_persistent_handle_list_t), 1);
	}

	list->used = 0;

	if (SUCCESS != zend_hash_init(&list->free, 0, NULL, NULL, 1)) {
		if (free_list) {
			pefree(list, 1);
		}
		list = NULL;
	}

	return list;
}

static int php_persistent_handle_apply_stat(void *p TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	php_persistent_handle_list_t **list = p;
	zval *zsubentry, *zentry = va_arg(argv, zval *);

	MAKE_STD_ZVAL(zsubentry);
	array_init(zsubentry);
	add_assoc_long_ex(zsubentry, ZEND_STRS("used"), (*list)->used);
	add_assoc_long_ex(zsubentry, ZEND_STRS("free"), zend_hash_num_elements(&(*list)->free));
	add_assoc_zval_ex(zentry, key->arKey, key->nKeyLength, zsubentry);

	return ZEND_HASH_APPLY_KEEP;
}

static int php_persistent_handle_apply_statall(void *p TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	php_persistent_handle_provider_t *provider = p;
	HashTable *ht = va_arg(argv, HashTable *);
	zval *zentry;

	MAKE_STD_ZVAL(zentry);
	array_init(zentry);

	zend_hash_apply_with_arguments(&provider->list.free TSRMLS_CC, php_persistent_handle_apply_stat, 1, zentry);
	zend_symtable_update(ht, key->arKey, key->nKeyLength, &zentry, sizeof(zval *), NULL);

	return ZEND_HASH_APPLY_KEEP;
}

static int php_persistent_handle_apply_cleanup_ex(void *pp, void *arg TSRMLS_DC)
{
	php_resource_factory_t *rf = arg;
	void **handle = pp;

#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "DESTROY: %p\n", *handle);
#endif
	php_resource_factory_handle_dtor(rf, *handle TSRMLS_CC);
	return ZEND_HASH_APPLY_REMOVE;
}

static int php_persistent_handle_apply_cleanup(void *pp, void *arg TSRMLS_DC)
{
	php_resource_factory_t *rf = arg;
	php_persistent_handle_list_t **listp = pp;

	zend_hash_apply_with_argument(&(*listp)->free, php_persistent_handle_apply_cleanup_ex, rf TSRMLS_CC);
	if ((*listp)->used) {
		return ZEND_HASH_APPLY_KEEP;
	}
	zend_hash_destroy(&(*listp)->free);
#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", *listp);
#endif
	pefree(*listp, 1);
	*listp = NULL;
	return ZEND_HASH_APPLY_REMOVE;
}

static inline void php_persistent_handle_list_dtor(php_persistent_handle_list_t *list, php_persistent_handle_provider_t *provider TSRMLS_DC)
{
#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "LSTDTOR: %p\n", list);
#endif
	zend_hash_apply_with_argument(&list->free, php_persistent_handle_apply_cleanup_ex, &provider->rf TSRMLS_CC);
	zend_hash_destroy(&list->free);
}

static inline void php_persistent_handle_list_free(php_persistent_handle_list_t **list, php_persistent_handle_provider_t *provider TSRMLS_DC)
{
	php_persistent_handle_list_dtor(*list, provider TSRMLS_CC);
#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "LSTFREE: %p\n", *list);
#endif
	pefree(*list, 1);
	*list = NULL;
}

static int php_persistent_handle_list_apply_dtor(void *listp, void *provider TSRMLS_DC)
{
	php_persistent_handle_list_free(listp, provider TSRMLS_CC);
	return ZEND_HASH_APPLY_REMOVE;
}

static inline php_persistent_handle_list_t *php_persistent_handle_list_find(php_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len TSRMLS_DC)
{
	php_persistent_handle_list_t **list, *new_list;

	if (SUCCESS == zend_symtable_find(&provider->list.free, ident_str, ident_len + 1, (void *) &list)) {
#if PHP_RAPHF_DEBUG_PHANDLES
		fprintf(stderr, "LSTFIND: %p\n", *list);
#endif
		return *list;
	}

	if ((new_list = php_persistent_handle_list_init(NULL))) {
		if (SUCCESS == zend_symtable_update(&provider->list.free, ident_str, ident_len + 1, (void *) &new_list, sizeof(php_persistent_handle_list_t *), (void *) &list)) {
#if PHP_RAPHF_DEBUG_PHANDLES
			fprintf(stderr, "LSTFIND: %p (new)\n", *list);
#endif
			return *list;
		}
		php_persistent_handle_list_free(&new_list, provider TSRMLS_CC);
	}

	return NULL;
}

static int php_persistent_handle_apply_cleanup_all(void *p TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	php_persistent_handle_provider_t *provider = p;
	const char *ident_str = va_arg(argv, const char *);
	size_t ident_len = va_arg(argv, size_t);
	php_persistent_handle_list_t *list;

	if (ident_str && ident_len) {
		if ((list = php_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
			zend_hash_apply_with_argument(&list->free, php_persistent_handle_apply_cleanup_ex, &provider->rf TSRMLS_CC);
		}
	} else {
		zend_hash_apply_with_argument(&provider->list.free, php_persistent_handle_apply_cleanup, &provider->rf TSRMLS_CC);
	}

	return ZEND_HASH_APPLY_KEEP;
}

static inline STATUS php_persistent_handle_do_acquire(php_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len, void *init_arg, void **handle, zend_bool *awakened TSRMLS_DC)
{
	ulong index;
	void **handle_ptr;
	php_persistent_handle_list_t *list;

	if ((list = php_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
		zend_hash_internal_pointer_end(&list->free);
		if (HASH_KEY_NON_EXISTANT != zend_hash_get_current_key(&list->free, NULL, &index, 0) && SUCCESS == zend_hash_get_current_data(&list->free, (void *) &handle_ptr)) {
			*awakened = 1;
			*handle = *handle_ptr;
			zend_hash_index_del(&list->free, index);
		} else {
			*handle = php_resource_factory_handle_ctor(&provider->rf, init_arg TSRMLS_CC);
		}
#if PHP_RAPHF_DEBUG_PHANDLES
		fprintf(stderr, "CREATED: %p\n", *handle);
#endif
		if (*handle) {
			++provider->list.used;
			++list->used;
			return SUCCESS;
		}
	} else {
		*handle = NULL;
	}

	return FAILURE;
}

static inline STATUS php_persistent_handle_do_release(php_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len, void **handle, zend_bool *retired TSRMLS_DC)
{
	php_persistent_handle_list_t *list;

	if ((list = php_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
		if (provider->list.used >= PHP_RAPHF_G->persistent_handle.limit) {
#if PHP_RAPHF_DEBUG_PHANDLES
			fprintf(stderr, "DESTROY: %p\n", *handle);
#endif
			php_resource_factory_handle_dtor(&provider->rf, *handle TSRMLS_CC);
		} else {
			if (SUCCESS != zend_hash_next_index_insert(&list->free, (void *) handle, sizeof(void *), NULL)) {
				return FAILURE;
			}
			*retired = 1;
		}

		--provider->list.used;
		--list->used;
		return SUCCESS;
	}

	return FAILURE;
}

static inline STATUS php_persistent_handle_do_accrete(php_persistent_handle_provider_t *provider, const char *ident_str, size_t ident_len, void *old_handle, void **new_handle TSRMLS_DC)
{
	php_persistent_handle_list_t *list;

	if ((*new_handle = php_resource_factory_handle_copy(&provider->rf, old_handle TSRMLS_CC))) {
		if ((list = php_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
			++list->used;
		}
		++provider->list.used;
		return SUCCESS;
	}
	return FAILURE;
}

static void php_persistent_handle_hash_dtor(void *p)
{
	php_persistent_handle_provider_t *provider = (php_persistent_handle_provider_t *) p;
	TSRMLS_FETCH();

	zend_hash_apply_with_argument(&provider->list.free, php_persistent_handle_list_apply_dtor, provider TSRMLS_CC);
	zend_hash_destroy(&provider->list.free);
	php_resource_factory_dtor(&provider->rf);
}

PHP_RAPHF_API STATUS php_persistent_handle_provide(const char *name_str, size_t name_len, php_resource_factory_ops_t *fops, void *data, void (*dtor)(void *))
{
	STATUS status = FAILURE;
	php_persistent_handle_provider_t provider;

	LOCK();
	if (php_persistent_handle_list_init(&provider.list)) {
		if (php_resource_factory_init(&provider.rf, fops, data, dtor)) {
#if PHP_RAPHF_DEBUG_PHANDLES
			fprintf(stderr, "PROVIDE: %s\n", name_str);
#endif

			if (SUCCESS == zend_symtable_update(&php_persistent_handle_hash, name_str, name_len+1, (void *) &provider, sizeof(php_persistent_handle_provider_t), NULL)) {
				status = SUCCESS;
			} else {
				php_resource_factory_dtor(&provider.rf);
			}
		}
	}
	UNLOCK();

	return status;
}

PHP_RAPHF_API php_persistent_handle_factory_t *php_persistent_handle_concede(php_persistent_handle_factory_t *a, const char *name_str, size_t name_len, const char *ident_str, size_t ident_len, php_persistent_handle_wakeup_t wakeup, php_persistent_handle_retire_t retire TSRMLS_DC)
{
	STATUS status = FAILURE;
	php_persistent_handle_factory_t *free_a = NULL;

	if (!a) {
		free_a = a = emalloc(sizeof(*a));
	}
	memset(a, 0, sizeof(*a));

	LOCK();
	status = zend_symtable_find(&php_persistent_handle_hash, name_str, name_len+1, (void *) &a->provider);
	UNLOCK();

	if (SUCCESS == status) {
		a->ident.str = estrndup(ident_str, ident_len);
		a->ident.len = ident_len;

		a->wakeup = wakeup;
		a->retire = retire;

		if (free_a) {
			a->free_on_abandon = 1;
		}
	} else {
		if (free_a) {
			efree(free_a);
		}
		a = NULL;
	}

#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "CONCEDE: %p (%s) (%s)\n", a ? a->provider : NULL, name_str, ident_str);
#endif

	return a;
}

PHP_RAPHF_API void php_persistent_handle_abandon(php_persistent_handle_factory_t *a)
{
	zend_bool f = a->free_on_abandon;

#if PHP_RAPHF_DEBUG_PHANDLES
	fprintf(stderr, "ABANDON: %p\n", a->provider);
#endif

	STR_FREE(a->ident.str);
	memset(a, 0, sizeof(*a));
	if (f) {
		efree(a);
	}
}

PHP_RAPHF_API void *php_persistent_handle_acquire(php_persistent_handle_factory_t *a, void *init_arg  TSRMLS_DC)
{
	void *handle = NULL;
	zend_bool awakened = 0;

	LOCK();
	php_persistent_handle_do_acquire(a->provider, a->ident.str, a->ident.len, init_arg, &handle, &awakened TSRMLS_CC);
	UNLOCK();

	if (awakened && a->wakeup) {
		a->wakeup(a, &handle TSRMLS_CC);
	}

	return handle;
}

PHP_RAPHF_API void *php_persistent_handle_accrete(php_persistent_handle_factory_t *a, void *handle TSRMLS_DC)
{
	void *new_handle = NULL;

	LOCK();
	php_persistent_handle_do_accrete(a->provider, a->ident.str, a->ident.len, handle, &new_handle TSRMLS_CC);
	UNLOCK();

	return new_handle;
}

PHP_RAPHF_API void php_persistent_handle_release(php_persistent_handle_factory_t *a, void *handle TSRMLS_DC)
{
	zend_bool retired = 0;

	LOCK();
	php_persistent_handle_do_release(a->provider, a->ident.str, a->ident.len, &handle, &retired TSRMLS_CC);
	UNLOCK();

	if (retired && a->retire) {
		a->retire(a, &handle TSRMLS_CC);
	}
}

PHP_RAPHF_API void php_persistent_handle_cleanup(const char *name_str, size_t name_len, const char *ident_str, size_t ident_len TSRMLS_DC)
{
	php_persistent_handle_provider_t *provider;
	php_persistent_handle_list_t *list;

	LOCK();
	if (name_str && name_len) {
		if (SUCCESS == zend_symtable_find(&php_persistent_handle_hash, name_str, name_len+1, (void *) &provider)) {
			if (ident_str && ident_len) {
				if ((list = php_persistent_handle_list_find(provider, ident_str, ident_len TSRMLS_CC))) {
					zend_hash_apply_with_argument(&list->free, php_persistent_handle_apply_cleanup_ex, &provider->rf TSRMLS_CC);
				}
			} else {
				zend_hash_apply_with_argument(&provider->list.free, php_persistent_handle_apply_cleanup, &provider->rf TSRMLS_CC);
			}
		}
	} else {
		zend_hash_apply_with_arguments(&php_persistent_handle_hash TSRMLS_CC, php_persistent_handle_apply_cleanup_all, 2, ident_str, ident_len);
	}
	UNLOCK();
}

PHP_RAPHF_API HashTable *php_persistent_handle_statall(HashTable *ht TSRMLS_DC)
{
	LOCK();
	if (zend_hash_num_elements(&php_persistent_handle_hash)) {
		if (!ht) {
			ALLOC_HASHTABLE(ht);
			zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		}
		zend_hash_apply_with_arguments(&php_persistent_handle_hash TSRMLS_CC, php_persistent_handle_apply_statall, 1, ht);
	} else if (ht) {
		ht = NULL;
	}
	UNLOCK();

	return ht;
}

static php_resource_factory_ops_t php_persistent_handle_resource_factory_ops = {
	(php_resource_factory_handle_ctor_t) php_persistent_handle_acquire,
	(php_resource_factory_handle_copy_t) php_persistent_handle_accrete,
	(php_resource_factory_handle_dtor_t) php_persistent_handle_release
};

PHP_RAPHF_API php_resource_factory_ops_t *php_persistent_handle_get_resource_factory_ops(void)
{
	return &php_persistent_handle_resource_factory_ops;
}

ZEND_BEGIN_ARG_INFO_EX(ai_raphf_stat_persistent_handles, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_FUNCTION(raphf_stat_persistent_handles)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		object_init(return_value);
		if (php_persistent_handle_statall(HASH_OF(return_value) TSRMLS_CC)) {
			return;
		}
		zval_dtor(return_value);
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_raphf_clean_persistent_handles, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, ident)
ZEND_END_ARG_INFO();
static PHP_FUNCTION(raphf_clean_persistent_handles)
{
	char *name_str = NULL, *ident_str = NULL;
	int name_len = 0, ident_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!s!", &name_str, &name_len, &ident_str, &ident_len)) {
		php_persistent_handle_cleanup(name_str, name_len, ident_str, ident_len TSRMLS_CC);
	}
}

static const zend_function_entry raphf_functions[] = {
	ZEND_NS_FENTRY("raphf", stat_persistent_handles, ZEND_FN(raphf_stat_persistent_handles), ai_raphf_stat_persistent_handles, 0)
	ZEND_NS_FENTRY("raphf", clean_persistent_handles, ZEND_FN(raphf_clean_persistent_handles), ai_raphf_clean_persistent_handles, 0)
	{0}
};

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("raphf.persistent_handle.limit", "-1", PHP_INI_SYSTEM, OnUpdateLong, persistent_handle.limit, zend_raphf_globals, raphf_globals)
PHP_INI_END()

static void php_raphf_init_globals(zend_raphf_globals *raphf_globals)
{
	raphf_globals->persistent_handle.limit = -1;
}

PHP_MINIT_FUNCTION(raphf)
{
	ZEND_INIT_MODULE_GLOBALS(raphf, php_raphf_init_globals, NULL);
	zend_hash_init(&php_persistent_handle_hash, 0, NULL, php_persistent_handle_hash_dtor, 1);
#ifdef ZTS
	php_persistent_handle_lock = tsrm_mutex_alloc();
#endif
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(raphf)
{
	zend_hash_destroy(&php_persistent_handle_hash);
#ifdef ZTS
	tsrm_mutex_free(php_persistent_handle_lock);
#endif
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

static int php_persistent_handle_apply_info_ex(void *p TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	php_persistent_handle_list_t **list = p;
	zend_hash_key *super_key = va_arg(argv, zend_hash_key *);
	char used[21], free[21];

	slprintf(used, sizeof(used), "%u", (*list)->used);
	slprintf(free, sizeof(free), "%d", zend_hash_num_elements(&(*list)->free));

	php_info_print_table_row(4, super_key->arKey, key->arKey, used, free);

	return ZEND_HASH_APPLY_KEEP;
}

static int php_persistent_handle_apply_info(void *p TSRMLS_DC, int argc, va_list argv, zend_hash_key *key)
{
	php_persistent_handle_provider_t *provider = p;

	zend_hash_apply_with_arguments(&provider->list.free TSRMLS_CC, php_persistent_handle_apply_info_ex, 1, key);

	return ZEND_HASH_APPLY_KEEP;
}

PHP_MINFO_FUNCTION(raphf)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Resource and persistent handle factory support", "enabled");
	php_info_print_table_row(2, "Extension version", PHP_RAPHF_VERSION);
	php_info_print_table_end();

	php_info_print_table_start();
	php_info_print_table_colspan_header(4, "Persistent handles");
	php_info_print_table_header(4, "Provider", "Ident", "Used", "Free");
	LOCK();
	zend_hash_apply_with_arguments(&php_persistent_handle_hash TSRMLS_CC, php_persistent_handle_apply_info, 0);
	UNLOCK();
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

zend_module_entry raphf_module_entry = {
	STANDARD_MODULE_HEADER,
	"raphf",
	raphf_functions,
	PHP_MINIT(raphf),
	PHP_MSHUTDOWN(raphf),
	NULL,
	NULL,
	PHP_MINFO(raphf),
	PHP_RAPHF_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_RAPHF
ZEND_GET_MODULE(raphf)
#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
