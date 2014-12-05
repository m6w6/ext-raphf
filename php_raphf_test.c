/*
    +--------------------------------------------------------------------+
    | PECL :: raphf                                                      |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2014, Michael Wallner <mike@php.net>                 |
    +--------------------------------------------------------------------+
*/

#include <php.h>

struct user_cb {
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
};

struct raphf_user {
	struct user_cb ctor;
	struct user_cb copy;
	struct user_cb dtor;
	struct {
		struct user_cb dtor;
		zval data;
	} data;
};

static inline void user_cb_addref(struct user_cb *cb)
{
	Z_ADDREF(cb->fci.function_name);
	if (cb->fci.object) {
		Z_ADDREF_P((zval *) cb->fci.object);
	}
}

static inline void user_cb_delref(struct user_cb *cb)
{
	if (cb->fci.object) {
		Z_DELREF_P((zval *) cb->fci.object);
	}
}

static void raphf_user_dtor(void *opaque)
{
	struct raphf_user *ru = opaque;

	zend_fcall_info_argn(&ru->data.dtor.fci, 1, &ru->data.data);
	zend_fcall_info_call(&ru->data.dtor.fci, &ru->data.dtor.fcc, NULL, NULL);
	zend_fcall_info_args_clear(&ru->data.dtor.fci, 1);
	user_cb_delref(&ru->data.dtor);
	zend_fcall_info_args_clear(&ru->ctor.fci, 1);
	user_cb_delref(&ru->ctor);
	zend_fcall_info_args_clear(&ru->copy.fci, 1);
	user_cb_delref(&ru->copy);
	zend_fcall_info_args_clear(&ru->dtor.fci, 1);
	user_cb_delref(&ru->dtor);
	memset(ru, 0, sizeof(*ru));
	efree(ru);
}

static void *user_ctor(void *opaque, void *init_arg TSRMLS_DC)
{
	struct raphf_user *ru = opaque;
	zval *zinit_arg = init_arg, *retval = ecalloc(1, sizeof(*retval));

	zend_fcall_info_argn(&ru->ctor.fci, 2, &ru->data.data, zinit_arg);
	zend_fcall_info_call(&ru->ctor.fci, &ru->ctor.fcc, retval, NULL);
	zend_fcall_info_args_clear(&ru->ctor.fci, 0);

	return retval;
}

static void *user_copy(void *opaque, void *handle TSRMLS_DC)
{
	struct raphf_user *ru = opaque;
	zval *zhandle = handle, *retval = ecalloc(1, sizeof(*retval));

	zend_fcall_info_argn(&ru->copy.fci, 2, &ru->data.data, zhandle);
	zend_fcall_info_call(&ru->copy.fci, &ru->copy.fcc, retval, NULL);
	zend_fcall_info_args_clear(&ru->copy.fci, 0);

	return retval;
}

static void user_dtor(void *opaque, void *handle TSRMLS_DC)
{
	struct raphf_user *ru = opaque;
	zval *zhandle = handle, retval;

	ZVAL_UNDEF(&retval);
	zend_fcall_info_argn(&ru->dtor.fci, 2, &ru->data.data, zhandle);
	zend_fcall_info_call(&ru->dtor.fci, &ru->dtor.fcc, &retval, NULL);
	zend_fcall_info_args_clear(&ru->dtor.fci, 0);
	if (!Z_ISUNDEF(retval)) {
		zval_ptr_dtor(&retval);
	}
}

static php_resource_factory_ops_t user_ops = {
		user_ctor,
		user_copy,
		user_dtor
};

static int raphf_user_le;

static void raphf_user_res_dtor(zend_resource *res TSRMLS_DC)
{
	php_resource_factory_free((void *) &res->ptr);
}

static PHP_FUNCTION(raphf_provide)
{
	struct raphf_user *ru;
	char *name_str;
	size_t name_len;
	zval *zdata;

	ru = ecalloc(1, sizeof(*ru));

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sfffzf",
			&name_str, &name_len,
			&ru->ctor.fci, &ru->ctor.fcc,
			&ru->copy.fci, &ru->copy.fcc,
			&ru->dtor.fci, &ru->dtor.fcc,
			&zdata,
			&ru->data.dtor.fci, &ru->data.dtor.fcc)) {
		efree(ru);
		return;
	}

	user_cb_addref(&ru->ctor);
	user_cb_addref(&ru->copy);
	user_cb_addref(&ru->dtor);
	user_cb_addref(&ru->data.dtor);

	ZVAL_COPY(&ru->data.data, zdata);

	if (SUCCESS != php_persistent_handle_provide(name_str, name_len,
			&user_ops, ru, raphf_user_dtor)) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

static PHP_FUNCTION(raphf_conceal)
{
	zend_string *name;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &name)) {
		return;
	}

	RETURN_BOOL(FAILURE != zend_hash_del(&PHP_RAPHF_G->persistent_handle.hash, name));
}

static PHP_FUNCTION(raphf_concede)
{
	char *name_str, *id_str;
	size_t name_len, id_len;
	php_persistent_handle_factory_t *pf;
	php_resource_factory_t *rf;
	php_resource_factory_ops_t *ops;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
			&name_str, &name_len, &id_str, &id_len)) {
		return;
	}

	ops = php_persistent_handle_get_resource_factory_ops();
	pf = php_persistent_handle_concede(NULL, name_str, name_len, id_str, id_len,
			NULL, NULL TSRMLS_CC);
	if (!pf) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Could not locate persistent handle factory '%s'", name_str);
		RETURN_FALSE;
	}
	rf = php_resource_factory_init(NULL, ops, pf,
			(void(*)(void*)) php_persistent_handle_abandon);
	if (!rf) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Could not create resource factory "
				"for persistent handle factory '%s'", name_str);
		RETURN_FALSE;
	}

	zend_register_resource(return_value, rf, raphf_user_le);
}

static PHP_FUNCTION(raphf_dispute)
{
	zval *zrf;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zrf)) {
		return;
	}

	RETURN_BOOL(SUCCESS == zend_list_close(Z_RES_P(zrf)));
}

static PHP_FUNCTION(raphf_handle_ctor)
{
	zval *zrf, *zrv, *zinit_arg;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz",
			&zrf, &zinit_arg)) {
		return;
	}

	zrv = php_resource_factory_handle_ctor(Z_RES_VAL_P(zrf), zinit_arg);
	RETVAL_ZVAL(zrv, 0, 0);
	efree(zrv);
}

static PHP_FUNCTION(raphf_handle_copy)
{
	zval *zrf, *zrv, *zhandle;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz",
			&zrf, &zhandle)) {
		return;
	}

	zrv = php_resource_factory_handle_copy(Z_RES_VAL_P(zrf), zhandle);
	RETVAL_ZVAL(zrv, 0, 0);
	efree(zrv);
}

static PHP_FUNCTION(raphf_handle_dtor)
{
	zval *zrf, *zhandle;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz",
			&zrf, &zhandle)) {
		return;
	}

	php_resource_factory_handle_dtor(Z_RES_VAL_P(zrf), zhandle);
}

static PHP_MINIT_FUNCTION(raphf_test)
{
	zend_register_long_constant(ZEND_STRL("RAPHF_TEST"), PHP_RAPHF_TEST, CONST_CS|CONST_PERSISTENT, module_number);
	raphf_user_le = zend_register_list_destructors_ex(raphf_user_res_dtor, NULL,
			"raphf_user", module_number);
	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(raphf_test)
{
	php_persistent_handle_cleanup(ZEND_STRL("test"), NULL, 0 TSRMLS_CC);
	return SUCCESS;
}
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
