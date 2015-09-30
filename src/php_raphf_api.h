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

#ifndef PHP_RAPHF_API_H
#define PHP_RAPHF_API_H

#include "php_raphf.h"

/**
 * A resource constructor.
 *
 * @param opaque is the \a data from php_persistent_handle_provide()
 * @param init_arg is the \a init_arg from php_resource_factory_init()
 * @return the created (persistent) handle
 */
typedef void *(*php_resource_factory_handle_ctor_t)(void *opaque, void *init_arg);

/**
 * The copy constructor of a resource.
 *
 * @param opaque the factory's data
 * @param handle the (persistent) handle to copy
 */
typedef void *(*php_resource_factory_handle_copy_t)(void *opaque, void *handle);

/**
 * The destructor of a resource.
 *
 * @param opaque the factory's data
 * @param handle the handle to destroy
 */
typedef void (*php_resource_factory_handle_dtor_t)(void *opaque, void *handle);

/**
 * The resource ops consisting of a ctor, a copy ctor and a dtor.
 *
 * Define this ops and register them with php_persistent_handle_provide()
 * in MINIT.
 */
typedef struct php_resource_factory_ops {
	/** The resource constructor */
	php_resource_factory_handle_ctor_t ctor;
	/** The resource's copy constructor */
	php_resource_factory_handle_copy_t copy;
	/** The resource's destructor */
	php_resource_factory_handle_dtor_t dtor;
} php_resource_factory_ops_t;

/**
 * The resource factory.
 */
typedef struct php_resource_factory {
	/** The resource ops */
	php_resource_factory_ops_t fops;
	/** Opaque user data */
	void *data;
	/** User data destructor */
	void (*dtor)(void *data);
	/** How often this factory is referenced */
	unsigned refcount;
} php_resource_factory_t;

/**
 * Initialize a resource factory.
 *
 * If you register a \a dtor for a resource factory used with a persistent
 * handle provider, be sure to call php_persistent_handle_cleanup() for your
 * registered provider in MSHUTDOWN, else the dtor will point to no longer
 * available memory if the extension has already been unloaded.
 *
 * @param f the factory to initialize; if NULL allocated on the heap
 * @param fops the resource ops to assign to the factory
 * @param data opaque user data; may be NULL
 * @param dtor a destructor for the data; may be NULL
 * @return \a f or an allocated resource factory
 */
PHP_RAPHF_API php_resource_factory_t *php_resource_factory_init(
		php_resource_factory_t *f, php_resource_factory_ops_t *fops, void *data,
		void (*dtor)(void *data));

/**
 * Increase the refcount of the resource factory.
 *
 * @param rf the resource factory
 * @return the new refcount
 */
PHP_RAPHF_API unsigned php_resource_factory_addref(php_resource_factory_t *rf);

/**
 * Destroy the resource factory.
 *
 * If the factory's refcount reaches 0, the \a dtor for \a data is called.
 *
 * @param f the resource factory
 */
PHP_RAPHF_API void php_resource_factory_dtor(php_resource_factory_t *f);

/**
 * Destroy and free the resource factory.
 *
 * Calls php_resource_factory_dtor() and frees \a f if the factory's refcount
 * reached 0.
 *
 * @param f the resource factory
 */
PHP_RAPHF_API void php_resource_factory_free(php_resource_factory_t **f);

/**
 * Construct a resource by the resource factory \a f
 *
 * @param f the resource factory
 * @param init_arg for the resource constructor
 * @return the new resource
 */
PHP_RAPHF_API void *php_resource_factory_handle_ctor(php_resource_factory_t *f,
		void *init_arg);

/**
 * Create a copy of the resource \a handle
 *
 * @param f the resource factory
 * @param handle the resource to copy
 * @return the copy
 */
PHP_RAPHF_API void *php_resource_factory_handle_copy(php_resource_factory_t *f,
		void *handle);

/**
 * Destroy (and free) the resource
 *
 * @param f the resource factory
 * @param handle the resource to destroy
 */
PHP_RAPHF_API void php_resource_factory_handle_dtor(php_resource_factory_t *f,
		void *handle);

/**
 * Persistent handles storage
 */
typedef struct php_persistent_handle_list {
	/** Storage of free resources */
	HashTable free;
	/** Count of acquired resources */
	ulong used;
} php_persistent_handle_list_t;

/**
 * Definition of a persistent handle provider.
 * Holds a resource factory an a persistent handle list.
 */
typedef struct php_persistent_handle_provider {
	 /**
	  * The list of free handles.
	  * Hash of "ident" => array(handles) entries. Persistent handles are
	  * acquired out of this list.
	  */
	php_persistent_handle_list_t list;

	/**
	 * The resource factory.
	 * New handles are created by this factory.
	 */
	php_resource_factory_t rf;
} php_persistent_handle_provider_t;

typedef struct php_persistent_handle_factory php_persistent_handle_factory_t;

/**
 * Wakeup the persistent handle on re-acquisition.
 */
typedef void (*php_persistent_handle_wakeup_t)(
		php_persistent_handle_factory_t *f, void **handle);
/**
 * Retire the persistent handle on release.
 */
typedef void (*php_persistent_handle_retire_t)(
		php_persistent_handle_factory_t *f, void **handle);

/**
 * Definition of a persistent handle factory.
 *
 * php_persistent_handle_concede() will return a pointer to a
 * php_persistent_handle_factory if a provider for the \a name has
 * been registered with php_persistent_handle_provide().
 */
struct php_persistent_handle_factory {
	/** The persistent handle provider */
	php_persistent_handle_provider_t *provider;
	/** The persistent handle wakeup routine; may be NULL */
	php_persistent_handle_wakeup_t wakeup;
	/** The persistent handle retire routine; may be NULL */
	php_persistent_handle_retire_t retire;

	/** The ident for which this factory manages resources */
	zend_string *ident;

	/** Whether it has to be free'd on php_persistent_handle_abandon() */
	unsigned free_on_abandon:1;
};

/**
 * Register a persistent handle provider in MINIT.
 *
 * Registers a factory provider for \a name_str with \a fops resource factory
 * ops. Call this in your MINIT.
 *
 * A php_resource_factory will be created with \a fops, \a data and \a dtor
 * and will be stored together with a php_persistent_handle_list in the global
 * raphf hash.
 *
 * A php_persistent_handle_factory can then be retrieved by
 * php_persistent_handle_concede() at runtime.
 *
 * @param name the provider name, e.g. "http\Client\Curl"
 * @param fops the resource factory ops
 * @param data opaque user data
 * @param dtor \a data destructor
 * @return SUCCESS/FAILURE
 */
PHP_RAPHF_API ZEND_RESULT_CODE php_persistent_handle_provide(
		zend_string *name, php_resource_factory_ops_t *fops,
		void *data, void (*dtor)(void *));

/**
 * Retrieve a persistent handle factory at runtime.
 *
 * If a persistent handle provider has been registered for \a name, a new
 * php_persistent_handle_factory creating resources in the \a ident
 * namespace will be constructed.
 *
 * The wakeup routine \a wakeup and the retire routine \a retire will be
 * assigned to the new php_persistent_handle_factory.
 *
 * @param a pointer to a factory; allocated on the heap if NULL
 * @param name the provider name, e.g. "http\Client\Curl"
 * @param ident the subsidiary namespace, e.g. "php.net:80"
 * @param wakeup any persistent handle wakeup routine
 * @param retire any persistent handle retire routine
 * @return \a a or an allocated persistent handle factory
 */
PHP_RAPHF_API php_persistent_handle_factory_t *php_persistent_handle_concede(
		php_persistent_handle_factory_t *a,
		zend_string *name, zend_string *ident,
		php_persistent_handle_wakeup_t wakeup,
		php_persistent_handle_retire_t retire);

/**
 * Abandon the persistent handle factory.
 *
 * Destroy a php_persistent_handle_factory created by
 * php_persistent_handle_concede(). If the memory for the factory was allocated,
 * it will automatically be free'd.
 *
 * @param a the persistent handle factory to destroy
 */
PHP_RAPHF_API void php_persistent_handle_abandon(
		php_persistent_handle_factory_t *a);

/**
 * Acquire a persistent handle.
 *
 * That is, either re-use a resource from the free list or create a new handle.
 *
 * If a handle is acquired from the free list, the
 * php_persistent_handle_factory::wakeup callback will be executed for that
 * handle.
 *
 * @param a the persistent handle factory
 * @param init_arg the \a init_arg for php_resource_factory_handle_ctor()
 * @return the acquired resource
 */
PHP_RAPHF_API void *php_persistent_handle_acquire(
		php_persistent_handle_factory_t *a, void *init_arg);

/**
 * Release a persistent handle.
 *
 * That is, either put it back into the free list for later re-use or clean it
 * up with php_resource_factory_handle_dtor().
 *
 * If a handle is put back into the free list, the
 * php_persistent_handle_factory::retire callback will be executed for that
 * handle.
 *
 * @param a the persistent handle factory
 * @param handle the handle to release
 */
PHP_RAPHF_API void php_persistent_handle_release(
		php_persistent_handle_factory_t *a, void *handle);

/**
 * Copy a persistent handle.
 *
 * Let the underlying resource factory copy the \a handle.
 *
 * @param a the persistent handle factory
 * @param handle the resource to accrete
 */
PHP_RAPHF_API void *php_persistent_handle_accrete(
		php_persistent_handle_factory_t *a, void *handle);

/**
 * Retrieve persistent handle resource factory ops.
 *
 * These ops can be used to mask a persistent handle factory as
 * resource factory itself, so you can transparently use the
 * resource factory API, both for persistent and non-persistent
 * ressources.
 *
 * Example:
 * \code{.c}
 * php_resource_factory_t *create_my_rf(zend_string *persistent_id)
 * {
 *     php_resource_factory_t *rf;
 *
 *     if (persistent_id) {
 *         php_persistent_handle_factory_t *pf;
 *         php_resource_factory_ops_t *ops;
 *         zend_string *ns = zend_string_init("my", 2, 1);
 *
 *         ops = php_persistent_handle_get_resource_factory_ops();
 *         pf = php_persistent_handle_concede(NULL, ns, persistent_id, NULL, NULL);
 *         rf = php_persistent_handle_resource_factory_init(NULL, pf);
 *         zend_string_release(ns);
 *     } else {
 *         rf = php_resource_factory_init(NULL, &myops, NULL, NULL);
 *     }
 *     return rf;
 * }
 * \endcode
 */
PHP_RAPHF_API php_resource_factory_ops_t *
php_persistent_handle_get_resource_factory_ops(void);

/**
 * Create a resource factory for persistent handles.
 *
 * This will create a resource factory with persistent handle ops, which wraps
 * the provided reource factory \a pf.
 *
 * @param a the persistent handle resource factory to initialize
 * @param pf the resource factory to wrap
 */
PHP_RAPHF_API php_resource_factory_t *
php_persistent_handle_resource_factory_init(php_resource_factory_t *a,
		php_persistent_handle_factory_t *pf);

/**
 * Check whether a resource factory is a persistent handle resource factory.
 *
 * @param a the resource factory to check
 */
PHP_RAPHF_API zend_bool php_resource_factory_is_persistent(
		php_resource_factory_t *a);

/**
 * Clean persistent handles up.
 *
 * Destroy persistent handles of provider \a name and in subsidiary
 * namespace \a ident.
 *
 * If \a name is NULL, all persistent handles of all providers with a
 * matching \a ident will be cleaned up.
 *
 * If \a identr is NULL all persistent handles of the provider will be
 * cleaned up.
 *
 * Ergo, if both, \a name and \a ident are NULL, then all
 * persistent handles will be cleaned up.
 *
 * You must call this in MSHUTDOWN, if your resource factory ops hold a
 * registered php_resource_factory::dtor, else the dtor will point to
 * memory not any more available if the extension has already been unloaded.
 *
 * @param name the provider name; may be NULL
 * @param ident the subsidiary namespace name; may be NULL
 */
PHP_RAPHF_API void php_persistent_handle_cleanup(zend_string *name,
		zend_string *ident);

/**
 * Retrieve statistics about the current process/thread's persistent handles.
 *
 * @return a HashTable like:
 * \code
 *     [
 *         "name" => [
 *             "ident" => [
 *                 "used" => 1,
 *                 "free" => 0,
 *             ]
 *         ]
 *     ]
 * \endcode
 */
PHP_RAPHF_API HashTable *php_persistent_handle_statall(HashTable *ht);

#endif	/* PHP_RAPHF_API_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
