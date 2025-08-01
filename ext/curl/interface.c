/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Sterling Hughes <sterling@php.net>                           |
   +----------------------------------------------------------------------+
*/

#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#include "Zend/zend_exceptions.h"

#include <stdio.h>
#include <string.h>

#ifdef PHP_WIN32
#include <winsock2.h>
#include <sys/types.h>
#endif

#include <curl/curl.h>
#include <curl/easy.h>

/* As of curl 7.11.1 this is no longer defined inside curl.h */
#ifndef HttpPost
#define HttpPost curl_httppost
#endif

#include "zend_smart_str.h"
#include "ext/standard/info.h"
#include "ext/standard/file.h"
#include "ext/standard/url.h"
#include "curl_private.h"

#ifdef __GNUC__
/* don't complain about deprecated CURLOPT_* we're exposing to PHP; we
   need to keep using those to avoid breaking PHP API compatibility */
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "zend_attributes.h"
#include "curl_arginfo.h"

ZEND_DECLARE_MODULE_GLOBALS(curl)

#define CAAL(s, v) add_assoc_long_ex(return_value, s, sizeof(s) - 1, (zend_long) v);
#define CAAD(s, v) add_assoc_double_ex(return_value, s, sizeof(s) - 1, (double) v);
#define CAAS(s, v) add_assoc_string_ex(return_value, s, sizeof(s) - 1, (char *) (v ? v : ""));
#define CAASTR(s, v) add_assoc_str_ex(return_value, s, sizeof(s) - 1, \
		v ? zend_string_copy(v) : ZSTR_EMPTY_ALLOC());
#define CAAZ(s, v) add_assoc_zval_ex(return_value, s, sizeof(s) -1 , (zval *) v);

#if defined(PHP_WIN32) || defined(__GNUC__)
# define php_curl_ret(__ret) RETVAL_FALSE; return __ret;
#else
# define php_curl_ret(__ret) RETVAL_FALSE; return;
#endif

static zend_result php_curl_option_str(php_curl *ch, zend_long option, const char *str, const size_t len)
{
	if (zend_char_has_nul_byte(str, len)) {
		zend_value_error("%s(): cURL option must not contain any null bytes", get_active_function_name());
		return FAILURE;
	}

	CURLcode error = curl_easy_setopt(ch->cp, option, str);
	SAVE_CURL_ERROR(ch, error);

	return error == CURLE_OK ? SUCCESS : FAILURE;
}

static zend_result php_curl_option_url(php_curl *ch, const zend_string *url) /* {{{ */
{
	/* Disable file:// if open_basedir are used */
	if (PG(open_basedir) && *PG(open_basedir)) {
		curl_easy_setopt(ch->cp, CURLOPT_PROTOCOLS, CURLPROTO_ALL & ~CURLPROTO_FILE);
	}

#ifdef PHP_WIN32
	if (
		zend_string_starts_with_literal_ci(url, "file://")
		&& '/' != ZSTR_VAL(url)[sizeof("file://") - 1]
		&& ZSTR_LEN(url) < MAXPATHLEN - 2
	) {
		char _tmp[MAXPATHLEN] = {0};

		memmove(_tmp, "file:///", sizeof("file:///") - 1);
		memmove(_tmp + sizeof("file:///") - 1, ZSTR_VAL(url) + sizeof("file://") - 1, ZSTR_LEN(url) - sizeof("file://") + 1);

		return php_curl_option_str(ch, CURLOPT_URL, _tmp, ZSTR_LEN(url) + 1);
	}
#endif

	return php_curl_option_str(ch, CURLOPT_URL, ZSTR_VAL(url), ZSTR_LEN(url));
}
/* }}} */

void _php_curl_verify_handlers(php_curl *ch, bool reporterror) /* {{{ */
{
	php_stream *stream;

	ZEND_ASSERT(ch);

	if (!Z_ISUNDEF(ch->handlers.std_err)) {
		stream = (php_stream *)zend_fetch_resource2_ex(&ch->handlers.std_err, NULL, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL, E_WARNING, "CURLOPT_STDERR resource has gone away, resetting to stderr");
			}
			zval_ptr_dtor(&ch->handlers.std_err);
			ZVAL_UNDEF(&ch->handlers.std_err);

			curl_easy_setopt(ch->cp, CURLOPT_STDERR, stderr);
		}
	}
	if (ch->handlers.read && !Z_ISUNDEF(ch->handlers.read->stream)) {
		stream = (php_stream *)zend_fetch_resource2_ex(&ch->handlers.read->stream, NULL, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL, E_WARNING, "CURLOPT_INFILE resource has gone away, resetting to default");
			}
			zval_ptr_dtor(&ch->handlers.read->stream);
			ZVAL_UNDEF(&ch->handlers.read->stream);
			ch->handlers.read->res = NULL;
			ch->handlers.read->fp = 0;

			curl_easy_setopt(ch->cp, CURLOPT_INFILE, (void *) ch);
		}
	}
	if (ch->handlers.write_header && !Z_ISUNDEF(ch->handlers.write_header->stream)) {
		stream = (php_stream *)zend_fetch_resource2_ex(&ch->handlers.write_header->stream, NULL, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL, E_WARNING, "CURLOPT_WRITEHEADER resource has gone away, resetting to default");
			}
			zval_ptr_dtor(&ch->handlers.write_header->stream);
			ZVAL_UNDEF(&ch->handlers.write_header->stream);
			ch->handlers.write_header->fp = 0;

			ch->handlers.write_header->method = PHP_CURL_IGNORE;
			curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER, (void *) ch);
		}
	}
	if (ch->handlers.write && !Z_ISUNDEF(ch->handlers.write->stream)) {
		stream = (php_stream *)zend_fetch_resource2_ex(&ch->handlers.write->stream, NULL, php_file_le_stream(), php_file_le_pstream());
		if (stream == NULL) {
			if (reporterror) {
				php_error_docref(NULL, E_WARNING, "CURLOPT_FILE resource has gone away, resetting to default");
			}
			zval_ptr_dtor(&ch->handlers.write->stream);
			ZVAL_UNDEF(&ch->handlers.write->stream);
			ch->handlers.write->fp = 0;

			ch->handlers.write->method = PHP_CURL_STDOUT;
			curl_easy_setopt(ch->cp, CURLOPT_FILE, (void *) ch);
		}
	}
	return;
}
/* }}} */

/* {{{ curl_module_entry */
zend_module_entry curl_module_entry = {
	STANDARD_MODULE_HEADER,
	"curl",
	ext_functions,
	PHP_MINIT(curl),
	PHP_MSHUTDOWN(curl),
	NULL,
	NULL,
	PHP_MINFO(curl),
	PHP_CURL_VERSION,
	PHP_MODULE_GLOBALS(curl),
	PHP_GINIT(curl),
	PHP_GSHUTDOWN(curl),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_CURL
ZEND_GET_MODULE (curl)
#endif

PHP_GINIT_FUNCTION(curl)
{
	zend_hash_init(&curl_globals->persistent_curlsh, 0, NULL, curl_share_free_persistent_curlsh, true);
	GC_MAKE_PERSISTENT_LOCAL(&curl_globals->persistent_curlsh);
}

PHP_GSHUTDOWN_FUNCTION(curl)
{
	zend_hash_destroy(&curl_globals->persistent_curlsh);
}

/* CurlHandle class */

zend_class_entry *curl_ce;
zend_class_entry *curl_share_ce;
zend_class_entry *curl_share_persistent_ce;
static zend_object_handlers curl_object_handlers;

static zend_object *curl_create_object(zend_class_entry *class_type);
static void curl_free_obj(zend_object *object);
static HashTable *curl_get_gc(zend_object *object, zval **table, int *n);
static zend_function *curl_get_constructor(zend_object *object);
static zend_object *curl_clone_obj(zend_object *object);
php_curl *init_curl_handle_into_zval(zval *curl);
static inline zend_result build_mime_structure_from_hash(php_curl *ch, zval *zpostfields);

/* {{{ PHP_INI_BEGIN */
PHP_INI_BEGIN()
	PHP_INI_ENTRY("curl.cainfo", "", PHP_INI_SYSTEM, NULL)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(curl)
{
	curl_version_info_data *d;
	char **p;
	char str[1024];
	size_t n = 0;

	d = curl_version_info(CURLVERSION_NOW);
	php_info_print_table_start();
	php_info_print_table_row(2, "cURL support",    "enabled");
	php_info_print_table_row(2, "cURL Information", d->version);
	snprintf(str, sizeof(str), "%d", d->age);
	php_info_print_table_row(2, "Age", str);

	/* To update on each new cURL release using src/main.c in cURL sources */
	/* make sure to sync this list with curl_version as well */
	if (d->features) {
		struct feat {
			const char *name;
			int bitmask;
		};

		unsigned int i;

		static const struct feat feats[] = {
			{"AsynchDNS", CURL_VERSION_ASYNCHDNS},
			{"CharConv", CURL_VERSION_CONV},
			{"Debug", CURL_VERSION_DEBUG},
			{"GSS-Negotiate", CURL_VERSION_GSSNEGOTIATE},
			{"IDN", CURL_VERSION_IDN},
			{"IPv6", CURL_VERSION_IPV6},
			{"krb4", CURL_VERSION_KERBEROS4},
			{"Largefile", CURL_VERSION_LARGEFILE},
			{"libz", CURL_VERSION_LIBZ},
			{"NTLM", CURL_VERSION_NTLM},
			{"NTLMWB", CURL_VERSION_NTLM_WB},
			{"SPNEGO", CURL_VERSION_SPNEGO},
			{"SSL",  CURL_VERSION_SSL},
			{"SSPI",  CURL_VERSION_SSPI},
			{"TLS-SRP", CURL_VERSION_TLSAUTH_SRP},
			{"HTTP2", CURL_VERSION_HTTP2},
			{"GSSAPI", CURL_VERSION_GSSAPI},
			{"KERBEROS5", CURL_VERSION_KERBEROS5},
			{"UNIX_SOCKETS", CURL_VERSION_UNIX_SOCKETS},
			{"PSL", CURL_VERSION_PSL},
			{"HTTPS_PROXY", CURL_VERSION_HTTPS_PROXY},
			{"MULTI_SSL", CURL_VERSION_MULTI_SSL},
			{"BROTLI", CURL_VERSION_BROTLI},
#if LIBCURL_VERSION_NUM >= 0x074001 /* Available since 7.64.1 */
			{"ALTSVC", CURL_VERSION_ALTSVC},
#endif
#if LIBCURL_VERSION_NUM >= 0x074200 /* Available since 7.66.0 */
			{"HTTP3", CURL_VERSION_HTTP3},
#endif
#if LIBCURL_VERSION_NUM >= 0x074800 /* Available since 7.72.0 */
			{"UNICODE", CURL_VERSION_UNICODE},
			{"ZSTD", CURL_VERSION_ZSTD},
#endif
#if LIBCURL_VERSION_NUM >= 0x074a00 /* Available since 7.74.0 */
			{"HSTS", CURL_VERSION_HSTS},
#endif
#if LIBCURL_VERSION_NUM >= 0x074c00 /* Available since 7.76.0 */
			{"GSASL", CURL_VERSION_GSASL},
#endif
			{NULL, 0}
		};

		php_info_print_table_row(1, "Features");
		for(i=0; i<sizeof(feats)/sizeof(feats[0]); i++) {
			if (feats[i].name) {
				php_info_print_table_row(2, feats[i].name, d->features & feats[i].bitmask ? "Yes" : "No");
			}
		}
	}

	n = 0;
	p = (char **) d->protocols;
	while (*p != NULL) {
			n += snprintf(str + n, sizeof(str) - n, "%s%s", *p, *(p + 1) != NULL ? ", " : "");
			p++;
	}
	php_info_print_table_row(2, "Protocols", str);

	php_info_print_table_row(2, "Host", d->host);

	if (d->ssl_version) {
		php_info_print_table_row(2, "SSL Version", d->ssl_version);
	}

	if (d->libz_version) {
		php_info_print_table_row(2, "ZLib Version", d->libz_version);
	}

#if defined(CURLVERSION_SECOND) && CURLVERSION_NOW >= CURLVERSION_SECOND
	if (d->ares) {
		php_info_print_table_row(2, "ZLib Version", d->ares);
	}
#endif

#if defined(CURLVERSION_THIRD) && CURLVERSION_NOW >= CURLVERSION_THIRD
	if (d->libidn) {
		php_info_print_table_row(2, "libIDN Version", d->libidn);
	}
#endif

	if (d->iconv_ver_num) {
		php_info_print_table_row(2, "IconV Version", d->iconv_ver_num);
	}

	if (d->libssh_version) {
		php_info_print_table_row(2, "libSSH Version", d->libssh_version);
	}

	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(curl)
{
	REGISTER_INI_ENTRIES();

	register_curl_symbols(module_number);

	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
		return FAILURE;
	}

	curl_ce = register_class_CurlHandle();
	curl_ce->create_object = curl_create_object;
	curl_ce->default_object_handlers = &curl_object_handlers;

	memcpy(&curl_object_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	curl_object_handlers.offset = XtOffsetOf(php_curl, std);
	curl_object_handlers.free_obj = curl_free_obj;
	curl_object_handlers.get_gc = curl_get_gc;
	curl_object_handlers.get_constructor = curl_get_constructor;
	curl_object_handlers.clone_obj = curl_clone_obj;
	curl_object_handlers.cast_object = curl_cast_object;
	curl_object_handlers.compare = zend_objects_not_comparable;

	curl_multi_ce = register_class_CurlMultiHandle();
	curl_multi_register_handlers();

	curl_share_ce = register_class_CurlShareHandle();
	curl_share_register_handlers();

	curl_share_persistent_ce = register_class_CurlSharePersistentHandle();
	curl_share_persistent_register_handlers();

	curlfile_register_class();

	return SUCCESS;
}
/* }}} */

/* CurlHandle class */

static zend_object *curl_create_object(zend_class_entry *class_type) {
	php_curl *intern = zend_object_alloc(sizeof(php_curl), class_type);

	zend_object_std_init(&intern->std, class_type);
	object_properties_init(&intern->std, class_type);

	return &intern->std;
}

static zend_function *curl_get_constructor(zend_object *object) {
	zend_throw_error(NULL, "Cannot directly construct CurlHandle, use curl_init() instead");
	return NULL;
}

static zend_object *curl_clone_obj(zend_object *object) {
	php_curl *ch;
	CURL *cp;
	zval *postfields;
	zend_object *clone_object;
	php_curl *clone_ch;

	clone_object = curl_create_object(curl_ce);
	clone_ch = curl_from_obj(clone_object);
	init_curl_handle(clone_ch);

	ch = curl_from_obj(object);
	cp = curl_easy_duphandle(ch->cp);
	if (!cp) {
		zend_throw_exception(NULL, "Failed to clone CurlHandle", 0);
		return &clone_ch->std;
	}

	clone_ch->cp = cp;
	_php_setup_easy_copy_handlers(clone_ch, ch);

	postfields = &clone_ch->postfields;
	if (Z_TYPE_P(postfields) != IS_UNDEF) {
		if (build_mime_structure_from_hash(clone_ch, postfields) == FAILURE) {
			zend_throw_exception(NULL, "Failed to clone CurlHandle", 0);
			return &clone_ch->std;
		}
	}

	return &clone_ch->std;
}

static HashTable *curl_get_gc(zend_object *object, zval **table, int *n)
{
	php_curl *curl = curl_from_obj(object);

	zend_get_gc_buffer *gc_buffer = zend_get_gc_buffer_create();

	zend_get_gc_buffer_add_zval(gc_buffer, &curl->postfields);
	if (curl->handlers.read) {
		if (ZEND_FCC_INITIALIZED(curl->handlers.read->fcc)) {
			zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.read->fcc);
		}
		zend_get_gc_buffer_add_zval(gc_buffer, &curl->handlers.read->stream);
	}

	if (curl->handlers.write) {
		if (ZEND_FCC_INITIALIZED(curl->handlers.write->fcc)) {
			zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.write->fcc);
		}
		zend_get_gc_buffer_add_zval(gc_buffer, &curl->handlers.write->stream);
	}

	if (curl->handlers.write_header) {
		if (ZEND_FCC_INITIALIZED(curl->handlers.write_header->fcc)) {
			zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.write_header->fcc);
		}
		zend_get_gc_buffer_add_zval(gc_buffer, &curl->handlers.write_header->stream);
	}

	if (ZEND_FCC_INITIALIZED(curl->handlers.progress)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.progress);
	}

	if (ZEND_FCC_INITIALIZED(curl->handlers.xferinfo)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.xferinfo);
	}

	if (ZEND_FCC_INITIALIZED(curl->handlers.fnmatch)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.fnmatch);
	}

	if (ZEND_FCC_INITIALIZED(curl->handlers.debug)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.debug);
	}

#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
	if (ZEND_FCC_INITIALIZED(curl->handlers.prereq)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.prereq);
	}
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
	if (ZEND_FCC_INITIALIZED(curl->handlers.sshhostkey)) {
		zend_get_gc_buffer_add_fcc(gc_buffer, &curl->handlers.sshhostkey);
	}
#endif

	zend_get_gc_buffer_add_zval(gc_buffer, &curl->handlers.std_err);
	zend_get_gc_buffer_add_zval(gc_buffer, &curl->private_data);

	zend_get_gc_buffer_use(gc_buffer, table, n);

	/* CurlHandle can never have properties as it's final and has strict-properties on.
	 * Avoid building a hash table. */
	return NULL;
}

zend_result curl_cast_object(zend_object *obj, zval *result, int type)
{
	if (type == IS_LONG) {
		/* For better backward compatibility, make (int) $curl_handle return the object ID,
		 * similar to how it previously returned the resource ID. */
		ZVAL_LONG(result, obj->handle);
		return SUCCESS;
	}

	return zend_std_cast_object_tostring(obj, result, type);
}

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(curl)
{
	curl_global_cleanup();
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ curl_write */
static size_t curl_write(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl *ch = (php_curl *) ctx;
	php_curl_write *write_handler = ch->handlers.write;
	size_t length = size * nmemb;

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_write() called\n");
	fprintf(stderr, "data = %s, size = %d, nmemb = %d, ctx = %x\n", data, size, nmemb, ctx);
#endif

	switch (write_handler->method) {
		case PHP_CURL_STDOUT:
			PHPWRITE(data, length);
			break;
		case PHP_CURL_FILE:
			return fwrite(data, size, nmemb, write_handler->fp);
		case PHP_CURL_RETURN:
			if (length > 0) {
				smart_str_appendl(&write_handler->buf, data, (int) length);
			}
			break;
		case PHP_CURL_USER: {
			zval argv[2];
			zval retval;

			GC_ADDREF(&ch->std);
			ZVAL_OBJ(&argv[0], &ch->std);
			ZVAL_STRINGL(&argv[1], data, length);

			ch->in_callback = true;
			zend_call_known_fcc(&write_handler->fcc, &retval, /* param_count */ 2, argv, /* named_params */ NULL);
			ch->in_callback = false;
			if (!Z_ISUNDEF(retval)) {
				_php_curl_verify_handlers(ch, /* reporterror */ true);
				/* TODO Check callback returns an int or something castable to int */
				length = php_curl_get_long(&retval);
			}

			zval_ptr_dtor(&argv[0]);
			zval_ptr_dtor(&argv[1]);
			break;
		}
	}

	return length;
}
/* }}} */

/* {{{ curl_fnmatch */
static int curl_fnmatch(void *ctx, const char *pattern, const char *string)
{
	php_curl *ch = (php_curl *) ctx;
	int rval = CURL_FNMATCHFUNC_FAIL;
	zval argv[3];
	zval retval;

	GC_ADDREF(&ch->std);
	ZVAL_OBJ(&argv[0], &ch->std);
	ZVAL_STRING(&argv[1], pattern);
	ZVAL_STRING(&argv[2], string);

	ch->in_callback = true;
	zend_call_known_fcc(&ch->handlers.fnmatch, &retval, /* param_count */ 3, argv, /* named_params */ NULL);
	ch->in_callback = false;

	if (!Z_ISUNDEF(retval)) {
		_php_curl_verify_handlers(ch, /* reporterror */ true);
		/* TODO Check callback returns an int or something castable to int */
		rval = php_curl_get_long(&retval);
	}
	zval_ptr_dtor(&argv[0]);
	zval_ptr_dtor(&argv[1]);
	zval_ptr_dtor(&argv[2]);
	return rval;
}
/* }}} */

/* {{{ curl_progress */
static size_t curl_progress(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	php_curl *ch = (php_curl *)clientp;
	size_t	rval = 0;

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_progress() called\n");
	fprintf(stderr, "clientp = %x, dltotal = %f, dlnow = %f, ultotal = %f, ulnow = %f\n", clientp, dltotal, dlnow, ultotal, ulnow);
#endif

	zval args[5];
	zval retval;

	GC_ADDREF(&ch->std);
	ZVAL_OBJ(&args[0], &ch->std);
	ZVAL_LONG(&args[1], (zend_long)dltotal);
	ZVAL_LONG(&args[2], (zend_long)dlnow);
	ZVAL_LONG(&args[3], (zend_long)ultotal);
	ZVAL_LONG(&args[4], (zend_long)ulnow);

	ch->in_callback = true;
	zend_call_known_fcc(&ch->handlers.progress, &retval, /* param_count */ 5, args, /* named_params */ NULL);
	ch->in_callback = false;

	if (!Z_ISUNDEF(retval)) {
		_php_curl_verify_handlers(ch, /* reporterror */ true);
		/* TODO Check callback returns an int or something castable to int */
		if (0 != php_curl_get_long(&retval)) {
			rval = 1;
		}
	}

	zval_ptr_dtor(&args[0]);
	return rval;
}
/* }}} */

/* {{{ curl_xferinfo */
static size_t curl_xferinfo(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	php_curl *ch = (php_curl *)clientp;
	size_t rval = 0;

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_xferinfo() called\n");
	fprintf(stderr, "clientp = %x, dltotal = %ld, dlnow = %ld, ultotal = %ld, ulnow = %ld\n", clientp, dltotal, dlnow, ultotal, ulnow);
#endif

	zval argv[5];
	zval retval;

	GC_ADDREF(&ch->std);
	ZVAL_OBJ(&argv[0], &ch->std);
	ZVAL_LONG(&argv[1], dltotal);
	ZVAL_LONG(&argv[2], dlnow);
	ZVAL_LONG(&argv[3], ultotal);
	ZVAL_LONG(&argv[4], ulnow);

	ch->in_callback = true;
	zend_call_known_fcc(&ch->handlers.xferinfo, &retval, /* param_count */ 5, argv, /* named_params */ NULL);
	ch->in_callback = false;

	if (!Z_ISUNDEF(retval)) {
		_php_curl_verify_handlers(ch, /* reporterror */ true);
		/* TODO Check callback returns an int or something castable to int */
		if (0 != php_curl_get_long(&retval)) {
			rval = 1;
		}
	}

	zval_ptr_dtor(&argv[0]);
	return rval;
}
/* }}} */

#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
static int curl_prereqfunction(void *clientp, char *conn_primary_ip, char *conn_local_ip, int conn_primary_port, int conn_local_port)
{
	php_curl *ch = (php_curl *)clientp;
	int rval = CURL_PREREQFUNC_OK;

	// when CURLOPT_PREREQFUNCTION is set to null, curl_prereqfunction still
	// gets called. Return CURL_PREREQFUNC_OK immediately in this case to avoid
	// zend_call_known_fcc() with an uninitialized FCC.
	if (!ZEND_FCC_INITIALIZED(ch->handlers.prereq)) {
    	return rval;
    }

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_prereqfunction() called\n");
	fprintf(stderr, "conn_primary_ip = %s, conn_local_ip = %s, conn_primary_port = %d, conn_local_port = %d\n", conn_primary_ip, conn_local_ip, conn_primary_port, conn_local_port);
#endif

	zval args[5];
	zval retval;

	GC_ADDREF(&ch->std);
	ZVAL_OBJ(&args[0], &ch->std);
	ZVAL_STRING(&args[1], conn_primary_ip);
	ZVAL_STRING(&args[2], conn_local_ip);
	ZVAL_LONG(&args[3], conn_primary_port);
	ZVAL_LONG(&args[4], conn_local_port);

	ch->in_callback = true;
	zend_call_known_fcc(&ch->handlers.prereq, &retval, /* param_count */ 5, args, /* named_params */ NULL);
	ch->in_callback = false;

	if (!Z_ISUNDEF(retval)) {
		_php_curl_verify_handlers(ch, /* reporterror */ true);
		if (Z_TYPE(retval) == IS_LONG) {
			zend_long retval_long = Z_LVAL(retval);
			if (retval_long == CURL_PREREQFUNC_OK || retval_long == CURL_PREREQFUNC_ABORT) {
				rval = retval_long;
			} else {
				zend_value_error("The CURLOPT_PREREQFUNCTION callback must return either CURL_PREREQFUNC_OK or CURL_PREREQFUNC_ABORT");
			}
		} else {
			zend_type_error("The CURLOPT_PREREQFUNCTION callback must return either CURL_PREREQFUNC_OK or CURL_PREREQFUNC_ABORT");
		}
	}

	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&args[2]);

	return rval;
}
#endif

#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
static int curl_ssh_hostkeyfunction(void *clientp, int keytype, const char *key, size_t keylen)
{
	php_curl *ch = (php_curl *)clientp;
	int rval = CURLKHMATCH_MISMATCH; /* cancel connection in case of an exception */

#if PHP_CURL_DEBUG
	fprintf(stderr, "curl_ssh_hostkeyfunction() called\n");
	fprintf(stderr, "clientp = %x, keytype = %d, key = %s, keylen = %zu\n", clientp, keytype, key, keylen);
#endif

	zval args[4];
	zval retval;

	GC_ADDREF(&ch->std);
	ZVAL_OBJ(&args[0], &ch->std);
	ZVAL_LONG(&args[1], keytype);
	ZVAL_STRINGL(&args[2], key, keylen);
	ZVAL_LONG(&args[3], keylen);

	ch->in_callback = true;
	zend_call_known_fcc(&ch->handlers.sshhostkey, &retval, /* param_count */ 4, args, /* named_params */ NULL);
	ch->in_callback = false;

	if (!Z_ISUNDEF(retval)) {
		_php_curl_verify_handlers(ch, /* reporterror */ true);
		if (Z_TYPE(retval) == IS_LONG) {
			zend_long retval_long = Z_LVAL(retval);
			if (retval_long == CURLKHMATCH_OK || retval_long == CURLKHMATCH_MISMATCH) {
				rval = retval_long;
			} else {
				zend_throw_error(NULL, "The CURLOPT_SSH_HOSTKEYFUNCTION callback must return either CURLKHMATCH_OK or CURLKHMATCH_MISMATCH");
			}
		} else {
			zend_throw_error(NULL, "The CURLOPT_SSH_HOSTKEYFUNCTION callback must return either CURLKHMATCH_OK or CURLKHMATCH_MISMATCH");
			zval_ptr_dtor(&retval);
		}
	}

	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[2]);
	return rval;
}
#endif

/* {{{ curl_read */
static size_t curl_read(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl *ch = (php_curl *)ctx;
	php_curl_read *read_handler = ch->handlers.read;
	size_t length = 0;

	switch (read_handler->method) {
		case PHP_CURL_DIRECT:
			if (read_handler->fp) {
				length = fread(data, size, nmemb, read_handler->fp);
			}
			break;
		case PHP_CURL_USER: {
			zval argv[3];
			zval retval;

			GC_ADDREF(&ch->std);
			ZVAL_OBJ(&argv[0], &ch->std);
			if (read_handler->res) {
				GC_ADDREF(read_handler->res);
				ZVAL_RES(&argv[1], read_handler->res);
			} else {
				ZVAL_NULL(&argv[1]);
			}
			ZVAL_LONG(&argv[2], (int)size * nmemb);

			ch->in_callback = true;
			zend_call_known_fcc(&read_handler->fcc, &retval, /* param_count */ 3, argv, /* named_params */ NULL);
			ch->in_callback = false;
			if (!Z_ISUNDEF(retval)) {
				_php_curl_verify_handlers(ch, /* reporterror */ true);
				if (Z_TYPE(retval) == IS_STRING) {
					length = MIN((size * nmemb), Z_STRLEN(retval));
					memcpy(data, Z_STRVAL(retval), length);
				} else if (Z_TYPE(retval) == IS_LONG) {
					length = Z_LVAL_P(&retval);
				}
				// TODO Do type error if invalid type?
				zval_ptr_dtor(&retval);
			}

			zval_ptr_dtor(&argv[0]);
			zval_ptr_dtor(&argv[1]);
			break;
		}
	}

	return length;
}
/* }}} */

/* {{{ curl_write_header */
static size_t curl_write_header(char *data, size_t size, size_t nmemb, void *ctx)
{
	php_curl *ch = (php_curl *) ctx;
	php_curl_write *write_handler = ch->handlers.write_header;
	size_t length = size * nmemb;

	switch (write_handler->method) {
		case PHP_CURL_STDOUT:
			/* Handle special case write when we're returning the entire transfer
			 */
			if (ch->handlers.write->method == PHP_CURL_RETURN && length > 0) {
				smart_str_appendl(&ch->handlers.write->buf, data, (int) length);
			} else {
				PHPWRITE(data, length);
			}
			break;
		case PHP_CURL_FILE:
			return fwrite(data, size, nmemb, write_handler->fp);
		case PHP_CURL_USER: {
			zval argv[2];
			zval retval;

			GC_ADDREF(&ch->std);
			ZVAL_OBJ(&argv[0], &ch->std);
			ZVAL_STRINGL(&argv[1], data, length);

			ch->in_callback = true;
			zend_call_known_fcc(&write_handler->fcc, &retval, /* param_count */ 2, argv, /* named_params */ NULL);
			ch->in_callback = false;
			if (!Z_ISUNDEF(retval)) {
				// TODO: Check for valid int type for return value
				_php_curl_verify_handlers(ch, /* reporterror */ true);
				length = php_curl_get_long(&retval);
			}
			zval_ptr_dtor(&argv[0]);
			zval_ptr_dtor(&argv[1]);
			break;
		}

		case PHP_CURL_IGNORE:
			return length;

		default:
			return -1;
	}

	return length;
}
/* }}} */

static int curl_debug(CURL *handle, curl_infotype type, char *data, size_t size, void *clientp) /* {{{ */
{
	php_curl *ch = (php_curl *)clientp;

    #if PHP_CURL_DEBUG
    	fprintf(stderr, "curl_debug() called\n");
    	fprintf(stderr, "type = %d, data = %s\n", type, data);
    #endif

	// Implicitly store the headers for compatibility with CURLINFO_HEADER_OUT
	// used as a Curl option. Previously, setting CURLINFO_HEADER_OUT set curl_debug
	// as the CURLOPT_DEBUGFUNCTION and stored the debug data when type is set to
	// CURLINFO_HEADER_OUT. For backward compatibility, we now store the headers
	// but also call the user-callback function if available.
    if (type == CURLINFO_HEADER_OUT) {
    	if (ch->header.str) {
    		zend_string_release_ex(ch->header.str, 0);
    	}
    	ch->header.str = zend_string_init(data, size, 0);
    }

    if (!ZEND_FCC_INITIALIZED(ch->handlers.debug)) {
       	return 0;
    }

    zval args[3];

    GC_ADDREF(&ch->std);
    ZVAL_OBJ(&args[0], &ch->std);
    ZVAL_LONG(&args[1], type);
    ZVAL_STRINGL(&args[2], data, size);

    ch->in_callback = true;
    zend_call_known_fcc(&ch->handlers.debug, NULL, /* param_count */ 3, args, /* named_params */ NULL);
    ch->in_callback = false;

    zval_ptr_dtor(&args[0]);
    zval_ptr_dtor(&args[2]);

    return 0;
}
/* }}} */

/* {{{ curl_free_post */
static void curl_free_post(void **post)
{
	curl_mime_free((curl_mime *)*post);
}
/* }}} */

struct mime_data_cb_arg {
	zend_string *filename;
	php_stream *stream;
};

/* {{{ curl_free_cb_arg */
static void curl_free_cb_arg(void **cb_arg_p)
{
	struct mime_data_cb_arg *cb_arg = (struct mime_data_cb_arg *) *cb_arg_p;

	ZEND_ASSERT(cb_arg->stream == NULL);
	zend_string_release(cb_arg->filename);
	efree(cb_arg);
}
/* }}} */

/* {{{ curl_free_slist */
static void curl_free_slist(zval *el)
{
	curl_slist_free_all(((struct curl_slist *)Z_PTR_P(el)));
}
/* }}} */

/* {{{ Return cURL version information. */
PHP_FUNCTION(curl_version)
{
	curl_version_info_data *d;

	ZEND_PARSE_PARAMETERS_NONE();

	d = curl_version_info(CURLVERSION_NOW);
	if (d == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);

	CAAL("version_number", d->version_num);
	CAAL("age", d->age);
	CAAL("features", d->features);
	/* Add an array of features */
	{
		struct feat {
			const char *name;
			int bitmask;
		};

		unsigned int i;
		zval feature_list;
		array_init(&feature_list);

		/* Sync this list with PHP_MINFO_FUNCTION(curl) as well */
		static const struct feat feats[] = {
			{"AsynchDNS", CURL_VERSION_ASYNCHDNS},
			{"CharConv", CURL_VERSION_CONV},
			{"Debug", CURL_VERSION_DEBUG},
			{"GSS-Negotiate", CURL_VERSION_GSSNEGOTIATE},
			{"IDN", CURL_VERSION_IDN},
			{"IPv6", CURL_VERSION_IPV6},
			{"krb4", CURL_VERSION_KERBEROS4},
			{"Largefile", CURL_VERSION_LARGEFILE},
			{"libz", CURL_VERSION_LIBZ},
			{"NTLM", CURL_VERSION_NTLM},
			{"NTLMWB", CURL_VERSION_NTLM_WB},
			{"SPNEGO", CURL_VERSION_SPNEGO},
			{"SSL",  CURL_VERSION_SSL},
			{"SSPI",  CURL_VERSION_SSPI},
			{"TLS-SRP", CURL_VERSION_TLSAUTH_SRP},
			{"HTTP2", CURL_VERSION_HTTP2},
			{"GSSAPI", CURL_VERSION_GSSAPI},
			{"KERBEROS5", CURL_VERSION_KERBEROS5},
			{"UNIX_SOCKETS", CURL_VERSION_UNIX_SOCKETS},
			{"PSL", CURL_VERSION_PSL},
			{"HTTPS_PROXY", CURL_VERSION_HTTPS_PROXY},
			{"MULTI_SSL", CURL_VERSION_MULTI_SSL},
			{"BROTLI", CURL_VERSION_BROTLI},
#if LIBCURL_VERSION_NUM >= 0x074001 /* Available since 7.64.1 */
			{"ALTSVC", CURL_VERSION_ALTSVC},
#endif
#if LIBCURL_VERSION_NUM >= 0x074200 /* Available since 7.66.0 */
			{"HTTP3", CURL_VERSION_HTTP3},
#endif
#if LIBCURL_VERSION_NUM >= 0x074800 /* Available since 7.72.0 */
			{"UNICODE", CURL_VERSION_UNICODE},
			{"ZSTD", CURL_VERSION_ZSTD},
#endif
#if LIBCURL_VERSION_NUM >= 0x074a00 /* Available since 7.74.0 */
			{"HSTS", CURL_VERSION_HSTS},
#endif
#if LIBCURL_VERSION_NUM >= 0x074c00 /* Available since 7.76.0 */
			{"GSASL", CURL_VERSION_GSASL},
#endif
		};

		for(i = 0; i < sizeof(feats) / sizeof(feats[0]); i++) {
			if (feats[i].name) {
				add_assoc_bool(&feature_list, feats[i].name, d->features & feats[i].bitmask ? true : false);
			}
		}

		CAAZ("feature_list", &feature_list);
	}
	CAAL("ssl_version_number", d->ssl_version_num);
	CAAS("version", d->version);
	CAAS("host", d->host);
	CAAS("ssl_version", d->ssl_version);
	CAAS("libz_version", d->libz_version);
	/* Add an array of protocols */
	{
		char **p = (char **) d->protocols;
		zval protocol_list;

		array_init(&protocol_list);

		while (*p != NULL) {
			add_next_index_string(&protocol_list, *p);
			p++;
		}
		CAAZ("protocols", &protocol_list);
	}
	if (d->age >= 1) {
		CAAS("ares", d->ares);
		CAAL("ares_num", d->ares_num);
	}
	if (d->age >= 2) {
		CAAS("libidn", d->libidn);
	}
	if (d->age >= 3) {
		CAAL("iconv_ver_num", d->iconv_ver_num);
		CAAS("libssh_version", d->libssh_version);
	}
	if (d->age >= 4) {
		CAAL("brotli_ver_num", d->brotli_ver_num);
		CAAS("brotli_version", d->brotli_version);
	}
}
/* }}} */

php_curl *init_curl_handle_into_zval(zval *curl)
{
	php_curl *ch;

	object_init_ex(curl, curl_ce);
	ch = Z_CURL_P(curl);

	init_curl_handle(ch);

	return ch;
}

void init_curl_handle(php_curl *ch)
{
	ch->to_free = ecalloc(1, sizeof(struct _php_curl_free));
	ch->handlers.write = ecalloc(1, sizeof(php_curl_write));
	ch->handlers.write_header = ecalloc(1, sizeof(php_curl_write));
	ch->handlers.read = ecalloc(1, sizeof(php_curl_read));
	ch->handlers.progress = empty_fcall_info_cache;
	ch->handlers.xferinfo = empty_fcall_info_cache;
	ch->handlers.fnmatch = empty_fcall_info_cache;
	ch->handlers.debug = empty_fcall_info_cache;
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
	ch->handlers.prereq = empty_fcall_info_cache;
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
	ch->handlers.sshhostkey = empty_fcall_info_cache;
#endif
	ch->clone = emalloc(sizeof(uint32_t));
	*ch->clone = 1;

	memset(&ch->err, 0, sizeof(struct _php_curl_error));

	zend_llist_init(&ch->to_free->post,  sizeof(struct HttpPost *), (llist_dtor_func_t)curl_free_post,   0);
	zend_llist_init(&ch->to_free->stream, sizeof(struct mime_data_cb_arg *), (llist_dtor_func_t)curl_free_cb_arg, 0);

	zend_hash_init(&ch->to_free->slist, 4, NULL, curl_free_slist, 0);
	ZVAL_UNDEF(&ch->postfields);
}

/* }}} */

/* {{{ create_certinfo */
static void create_certinfo(struct curl_certinfo *ci, zval *listcode)
{
	int i;

	if (ci) {
		zval certhash;

		for (i=0; i<ci->num_of_certs; i++) {
			struct curl_slist *slist;

			array_init(&certhash);
			for (slist = ci->certinfo[i]; slist; slist = slist->next) {
				char s[64];
				char *tmp;
				strncpy(s, slist->data, sizeof(s));
				s[sizeof(s)-1] = '\0';
				tmp = memchr(s, ':', sizeof(s));
				if(tmp) {
					*tmp = '\0';
					size_t len = strlen(s);
					add_assoc_string(&certhash, s, &slist->data[len+1]);
				} else {
					php_error_docref(NULL, E_WARNING, "Could not extract hash key from certificate info");
				}
			}
			add_next_index_zval(listcode, &certhash);
		}
	}
}
/* }}} */

/* {{{ _php_curl_set_default_options()
   Set default options for a handle */
static void _php_curl_set_default_options(php_curl *ch)
{
	char *cainfo;

	curl_easy_setopt(ch->cp, CURLOPT_NOPROGRESS,        1);
	curl_easy_setopt(ch->cp, CURLOPT_VERBOSE,           0);
	curl_easy_setopt(ch->cp, CURLOPT_ERRORBUFFER,       ch->err.str);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEFUNCTION,     curl_write);
	curl_easy_setopt(ch->cp, CURLOPT_FILE,              (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_READFUNCTION,      curl_read);
	curl_easy_setopt(ch->cp, CURLOPT_INFILE,            (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_HEADERFUNCTION,    curl_write_header);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER,       (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_DNS_CACHE_TIMEOUT, 120);
	curl_easy_setopt(ch->cp, CURLOPT_MAXREDIRS, 20); /* prevent infinite redirects */

	cainfo = INI_STR("openssl.cafile");
	if (!(cainfo && cainfo[0] != '\0')) {
		cainfo = INI_STR("curl.cainfo");
	}
	if (cainfo && cainfo[0] != '\0') {
		curl_easy_setopt(ch->cp, CURLOPT_CAINFO, cainfo);
	}

#ifdef ZTS
	curl_easy_setopt(ch->cp, CURLOPT_NOSIGNAL, 1);
#endif
}
/* }}} */

/* {{{ Initialize a cURL session */
PHP_FUNCTION(curl_init)
{
	php_curl *ch;
	CURL 	 *cp;
	zend_string *url = NULL;

	ZEND_PARSE_PARAMETERS_START(0,1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STR_OR_NULL(url)
	ZEND_PARSE_PARAMETERS_END();

	cp = curl_easy_init();
	if (!cp) {
		php_error_docref(NULL, E_WARNING, "Could not initialize a new cURL handle");
		RETURN_FALSE;
	}

	ch = init_curl_handle_into_zval(return_value);

	ch->cp = cp;

	ch->handlers.write->method = PHP_CURL_STDOUT;
	ch->handlers.read->method  = PHP_CURL_DIRECT;
	ch->handlers.write_header->method = PHP_CURL_IGNORE;

	_php_curl_set_default_options(ch);

	if (url) {
		if (php_curl_option_url(ch, url) == FAILURE) {
			zval_ptr_dtor(return_value);
			RETURN_FALSE;
		}
	}
}
/* }}} */

static void php_curl_copy_fcc_with_option(php_curl *ch, CURLoption option, zend_fcall_info_cache *target_fcc, zend_fcall_info_cache *source_fcc)
{
	if (ZEND_FCC_INITIALIZED(*source_fcc)) {
		zend_fcc_dup(target_fcc, source_fcc);
		curl_easy_setopt(ch->cp, option, (void *) ch);
	}
}

void _php_setup_easy_copy_handlers(php_curl *ch, php_curl *source)
{
	if (!Z_ISUNDEF(source->handlers.write->stream)) {
		Z_ADDREF(source->handlers.write->stream);
	}
	ch->handlers.write->stream = source->handlers.write->stream;
	ch->handlers.write->method = source->handlers.write->method;
	if (!Z_ISUNDEF(source->handlers.read->stream)) {
		Z_ADDREF(source->handlers.read->stream);
	}
	ch->handlers.read->stream  = source->handlers.read->stream;
	ch->handlers.read->method  = source->handlers.read->method;
	ch->handlers.write_header->method = source->handlers.write_header->method;
	if (!Z_ISUNDEF(source->handlers.write_header->stream)) {
		Z_ADDREF(source->handlers.write_header->stream);
	}
	ch->handlers.write_header->stream = source->handlers.write_header->stream;

	ch->handlers.write->fp = source->handlers.write->fp;
	ch->handlers.write_header->fp = source->handlers.write_header->fp;
	ch->handlers.read->fp = source->handlers.read->fp;
	ch->handlers.read->res = source->handlers.read->res;

	if (ZEND_FCC_INITIALIZED(source->handlers.read->fcc)) {
		zend_fcc_dup(&ch->handlers.read->fcc, &source->handlers.read->fcc);
	}
	if (ZEND_FCC_INITIALIZED(source->handlers.write->fcc)) {
		zend_fcc_dup(&ch->handlers.write->fcc, &source->handlers.write->fcc);
	}
	if (ZEND_FCC_INITIALIZED(source->handlers.write_header->fcc)) {
		zend_fcc_dup(&ch->handlers.write_header->fcc, &source->handlers.write_header->fcc);
	}

	curl_easy_setopt(ch->cp, CURLOPT_ERRORBUFFER,       ch->err.str);
	curl_easy_setopt(ch->cp, CURLOPT_FILE,              (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_INFILE,            (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_WRITEHEADER,       (void *) ch);
	curl_easy_setopt(ch->cp, CURLOPT_DEBUGDATA,         (void *) ch);

	php_curl_copy_fcc_with_option(ch, CURLOPT_PROGRESSDATA, &ch->handlers.progress, &source->handlers.progress);
	php_curl_copy_fcc_with_option(ch, CURLOPT_XFERINFODATA, &ch->handlers.xferinfo, &source->handlers.xferinfo);
	php_curl_copy_fcc_with_option(ch, CURLOPT_FNMATCH_DATA, &ch->handlers.fnmatch, &source->handlers.fnmatch);
	php_curl_copy_fcc_with_option(ch, CURLOPT_DEBUGDATA, &ch->handlers.debug, &source->handlers.debug);
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
	php_curl_copy_fcc_with_option(ch, CURLOPT_PREREQDATA, &ch->handlers.prereq, &source->handlers.prereq);
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
	php_curl_copy_fcc_with_option(ch, CURLOPT_SSH_HOSTKEYDATA, &ch->handlers.sshhostkey, &source->handlers.sshhostkey);
#endif

	ZVAL_COPY(&ch->private_data, &source->private_data);

	efree(ch->to_free);
	ch->to_free = source->to_free;
	efree(ch->clone);
	ch->clone = source->clone;

	/* Keep track of cloned copies to avoid invoking curl destructors for every clone */
	(*source->clone)++;
}

zend_long php_curl_get_long(zval *zv)
{
	if (EXPECTED(Z_TYPE_P(zv) == IS_LONG)) {
		return Z_LVAL_P(zv);
	} else {
		zend_long ret = zval_get_long(zv);
		zval_ptr_dtor(zv);
		return ret;
	}
}

static size_t read_cb(char *buffer, size_t size, size_t nitems, void *arg) /* {{{ */
{
	struct mime_data_cb_arg *cb_arg = (struct mime_data_cb_arg *) arg;
	ssize_t numread;

	if (cb_arg->stream == NULL) {
		if (!(cb_arg->stream = php_stream_open_wrapper(ZSTR_VAL(cb_arg->filename), "rb", IGNORE_PATH, NULL))) {
			return CURL_READFUNC_ABORT;
		}
	}
	numread = php_stream_read(cb_arg->stream, buffer, nitems * size);
	if (numread < 0) {
		php_stream_close(cb_arg->stream);
		cb_arg->stream = NULL;
		return CURL_READFUNC_ABORT;
	}
	return numread;
}
/* }}} */

static int seek_cb(void *arg, curl_off_t offset, int origin) /* {{{ */
{
	struct mime_data_cb_arg *cb_arg = (struct mime_data_cb_arg *) arg;
	int res;

	if (cb_arg->stream == NULL) {
		return CURL_SEEKFUNC_CANTSEEK;
	}
	res = php_stream_seek(cb_arg->stream, offset, origin);
	return res == SUCCESS ? CURL_SEEKFUNC_OK : CURL_SEEKFUNC_CANTSEEK;
}
/* }}} */

static void free_cb(void *arg) /* {{{ */
{
	struct mime_data_cb_arg *cb_arg = (struct mime_data_cb_arg *) arg;

	if (cb_arg->stream != NULL) {
		php_stream_close(cb_arg->stream);
		cb_arg->stream = NULL;
	}
}
/* }}} */

static inline CURLcode add_simple_field(curl_mime *mime, zend_string *string_key, zval *current)
{
	CURLcode error = CURLE_OK;
	curl_mimepart *part;
	CURLcode form_error;
	zend_string *postval, *tmp_postval;

	postval = zval_get_tmp_string(current, &tmp_postval);

	part = curl_mime_addpart(mime);
	if (part == NULL) {
		zend_tmp_string_release(tmp_postval);
		return CURLE_OUT_OF_MEMORY;
	}
	if ((form_error = curl_mime_name(part, ZSTR_VAL(string_key))) != CURLE_OK
		|| (form_error = curl_mime_data(part, ZSTR_VAL(postval), ZSTR_LEN(postval))) != CURLE_OK) {
		error = form_error;
	}

	zend_tmp_string_release(tmp_postval);

	return error;
}

static inline zend_result build_mime_structure_from_hash(php_curl *ch, zval *zpostfields) /* {{{ */
{
	HashTable *postfields = Z_ARRVAL_P(zpostfields);
	CURLcode error = CURLE_OK;
	zval *current;
	zend_string *string_key;
	zend_ulong num_key;
	curl_mime *mime = NULL;
	curl_mimepart *part;
	CURLcode form_error;

	if (zend_hash_num_elements(postfields) > 0) {
		mime = curl_mime_init(ch->cp);
		if (mime == NULL) {
			return FAILURE;
		}
	}

	ZEND_HASH_FOREACH_KEY_VAL(postfields, num_key, string_key, current) {
		zend_string *postval;
		/* Pretend we have a string_key here */
		if (!string_key) {
			string_key = zend_long_to_str(num_key);
		} else {
			zend_string_addref(string_key);
		}

		ZVAL_DEREF(current);
		if (Z_TYPE_P(current) == IS_OBJECT &&
				instanceof_function(Z_OBJCE_P(current), curl_CURLFile_class)) {
			/* new-style file upload */
			zval *prop, rv;
			char *type = NULL, *filename = NULL;
			struct mime_data_cb_arg *cb_arg;
			php_stream_statbuf ssb;
			size_t filesize = -1;
			curl_seek_callback seekfunc = seek_cb;

			prop = zend_read_property_ex(curl_CURLFile_class, Z_OBJ_P(current), ZSTR_KNOWN(ZEND_STR_NAME), /* silent */ false, &rv);
			ZVAL_DEREF(prop);
			if (Z_TYPE_P(prop) != IS_STRING) {
				php_error_docref(NULL, E_WARNING, "Invalid filename for key %s", ZSTR_VAL(string_key));
			} else {
				postval = Z_STR_P(prop);

				if (php_check_open_basedir(ZSTR_VAL(postval))) {
					goto out_string;
				}

				prop = zend_read_property(curl_CURLFile_class, Z_OBJ_P(current), "mime", sizeof("mime")-1, 0, &rv);
				ZVAL_DEREF(prop);
				if (Z_TYPE_P(prop) == IS_STRING && Z_STRLEN_P(prop) > 0) {
					type = Z_STRVAL_P(prop);
				}
				prop = zend_read_property(curl_CURLFile_class, Z_OBJ_P(current), "postname", sizeof("postname")-1, 0, &rv);
				ZVAL_DEREF(prop);
				if (Z_TYPE_P(prop) == IS_STRING && Z_STRLEN_P(prop) > 0) {
					filename = Z_STRVAL_P(prop);
				}

				zval_ptr_dtor(&ch->postfields);
				ZVAL_COPY(&ch->postfields, zpostfields);

				php_stream *stream;
				if ((stream = php_stream_open_wrapper(ZSTR_VAL(postval), "rb", STREAM_MUST_SEEK, NULL))) {
					if (!stream->readfilters.head && !php_stream_stat(stream, &ssb)) {
						filesize = ssb.sb.st_size;
					}
				} else {
					seekfunc = NULL;
				}

				part = curl_mime_addpart(mime);
				if (part == NULL) {
					if (stream) {
						php_stream_close(stream);
					}
					goto out_string;
				}

				cb_arg = emalloc(sizeof *cb_arg);
				cb_arg->filename = zend_string_copy(postval);
				cb_arg->stream = stream;

				if ((form_error = curl_mime_name(part, ZSTR_VAL(string_key))) != CURLE_OK
					|| (form_error = curl_mime_data_cb(part, filesize, read_cb, seekfunc, free_cb, cb_arg)) != CURLE_OK
					|| (form_error = curl_mime_filename(part, filename ? filename : ZSTR_VAL(postval))) != CURLE_OK
					|| (form_error = curl_mime_type(part, type ? type : "application/octet-stream")) != CURLE_OK) {
					error = form_error;
				}
				zend_llist_add_element(&ch->to_free->stream, &cb_arg);
			}

			zend_string_release_ex(string_key, 0);
			continue;
		}

		if (Z_TYPE_P(current) == IS_OBJECT && instanceof_function(Z_OBJCE_P(current), curl_CURLStringFile_class)) {
			/* new-style file upload from string */
			zval *prop, rv;
			char *type = NULL, *filename = NULL;

			prop = zend_read_property(curl_CURLStringFile_class, Z_OBJ_P(current), "postname", sizeof("postname")-1, 0, &rv);
			if (EG(exception)) {
				goto out_string;
			}
			ZVAL_DEREF(prop);
			ZEND_ASSERT(Z_TYPE_P(prop) == IS_STRING);

			filename = Z_STRVAL_P(prop);

			prop = zend_read_property(curl_CURLStringFile_class, Z_OBJ_P(current), "mime", sizeof("mime")-1, 0, &rv);
			if (EG(exception)) {
				goto out_string;
			}
			ZVAL_DEREF(prop);
			ZEND_ASSERT(Z_TYPE_P(prop) == IS_STRING);

			type = Z_STRVAL_P(prop);

			prop = zend_read_property(curl_CURLStringFile_class, Z_OBJ_P(current), "data", sizeof("data")-1, 0, &rv);
			if (EG(exception)) {
				goto out_string;
			}
			ZVAL_DEREF(prop);
			ZEND_ASSERT(Z_TYPE_P(prop) == IS_STRING);

			postval = Z_STR_P(prop);

			zval_ptr_dtor(&ch->postfields);
			ZVAL_COPY(&ch->postfields, zpostfields);

			part = curl_mime_addpart(mime);
			if (part == NULL) {
				goto out_string;
			}
			if ((form_error = curl_mime_name(part, ZSTR_VAL(string_key))) != CURLE_OK
				|| (form_error = curl_mime_data(part, ZSTR_VAL(postval), ZSTR_LEN(postval))) != CURLE_OK
				|| (form_error = curl_mime_filename(part, filename)) != CURLE_OK
				|| (form_error = curl_mime_type(part, type)) != CURLE_OK) {
				error = form_error;
			}

			zend_string_release_ex(string_key, 0);
			continue;
		}

		if (Z_TYPE_P(current) == IS_ARRAY) {
			zval *current_element;

			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(current), current_element) {
				add_simple_field(mime, string_key, current_element);
			} ZEND_HASH_FOREACH_END();

			zend_string_release_ex(string_key, 0);
			continue;
		}

		add_simple_field(mime, string_key, current);

		zend_string_release_ex(string_key, 0);
	} ZEND_HASH_FOREACH_END();

	SAVE_CURL_ERROR(ch, error);
	if (error != CURLE_OK) {
		goto out_mime;
	}

	if ((*ch->clone) == 1) {
		zend_llist_clean(&ch->to_free->post);
	}
	zend_llist_add_element(&ch->to_free->post, &mime);
	error = curl_easy_setopt(ch->cp, CURLOPT_MIMEPOST, mime);

	SAVE_CURL_ERROR(ch, error);
	return error == CURLE_OK ? SUCCESS : FAILURE;

out_string:
	zend_string_release_ex(string_key, false);
out_mime:
	curl_mime_free(mime);
	return FAILURE;
}
/* }}} */

/* {{{ Copy a cURL handle along with all of it's preferences */
PHP_FUNCTION(curl_copy_handle)
{
	php_curl	*ch;
	CURL		*cp;
	zval		*zid;
	php_curl	*dupch;
	zval		*postfields;

	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	cp = curl_easy_duphandle(ch->cp);
	if (!cp) {
		php_error_docref(NULL, E_WARNING, "Cannot duplicate cURL handle");
		RETURN_FALSE;
	}

	dupch = init_curl_handle_into_zval(return_value);
	dupch->cp = cp;

	_php_setup_easy_copy_handlers(dupch, ch);

	postfields = &ch->postfields;
	if (Z_TYPE_P(postfields) != IS_UNDEF) {
		if (build_mime_structure_from_hash(dupch, postfields) == FAILURE) {
			zval_ptr_dtor(return_value);
			php_error_docref(NULL, E_WARNING, "Cannot rebuild mime structure");
			RETURN_FALSE;
		}
	}
}
/* }}} */

static bool php_curl_set_callable_handler(zend_fcall_info_cache *const handler_fcc, zval *callable, bool is_array_config, const char *option_name)
{
	if (ZEND_FCC_INITIALIZED(*handler_fcc)) {
		zend_fcc_dtor(handler_fcc);
	}

	if (Z_TYPE_P(callable) == IS_NULL) {
		return true;
	}

	char *error = NULL;
	if (UNEXPECTED(!zend_is_callable_ex(callable, /* object */ NULL, /* check_flags */ 0, /* callable_name */ NULL, handler_fcc, /* error */ &error))) {
		if (!EG(exception)) {
			zend_argument_type_error(2 + !is_array_config, "must be a valid callback for option %s, %s", option_name, error);
		}
		efree(error);
		return false;
	}
	zend_fcc_addref(handler_fcc);
	return true;
}


#define HANDLE_CURL_OPTION_CALLABLE_PHP_CURL_USER(curl_ptr, constant_no_function, handler_type, default_method) \
	case constant_no_function##FUNCTION: { \
		bool result = php_curl_set_callable_handler(&curl_ptr->handlers.handler_type->fcc, zvalue, is_array_config, #constant_no_function "FUNCTION"); \
		if (!result) { \
			curl_ptr->handlers.handler_type->method = default_method; \
			return FAILURE; \
		} \
		if (!ZEND_FCC_INITIALIZED(curl_ptr->handlers.handler_type->fcc)) { \
			curl_ptr->handlers.handler_type->method = default_method; \
			return SUCCESS; \
		} \
		curl_ptr->handlers.handler_type->method = PHP_CURL_USER; \
		break; \
	}

#define HANDLE_CURL_OPTION_CALLABLE(curl_ptr, constant_no_function, handler_fcc, c_callback) \
	case constant_no_function##FUNCTION: { \
		bool result = php_curl_set_callable_handler(&curl_ptr->handler_fcc, zvalue, is_array_config, #constant_no_function "FUNCTION"); \
		if (!result) { \
			return FAILURE; \
		} \
		curl_easy_setopt(curl_ptr->cp, constant_no_function##FUNCTION, (c_callback)); \
		curl_easy_setopt(curl_ptr->cp, constant_no_function##DATA, curl_ptr); \
		break; \
	}

static zend_result _php_curl_setopt(php_curl *ch, zend_long option, zval *zvalue, bool is_array_config) /* {{{ */
{
	CURLcode error = CURLE_OK;
	zend_long lval;

	switch (option) {
		/* Callable options */
		HANDLE_CURL_OPTION_CALLABLE_PHP_CURL_USER(ch, CURLOPT_WRITE, write, PHP_CURL_STDOUT);
		HANDLE_CURL_OPTION_CALLABLE_PHP_CURL_USER(ch, CURLOPT_HEADER, write_header, PHP_CURL_IGNORE);
		HANDLE_CURL_OPTION_CALLABLE_PHP_CURL_USER(ch, CURLOPT_READ, read, PHP_CURL_DIRECT);

		HANDLE_CURL_OPTION_CALLABLE(ch, CURLOPT_PROGRESS, handlers.progress, curl_progress);
		HANDLE_CURL_OPTION_CALLABLE(ch, CURLOPT_XFERINFO, handlers.xferinfo, curl_xferinfo);
		HANDLE_CURL_OPTION_CALLABLE(ch, CURLOPT_FNMATCH_, handlers.fnmatch, curl_fnmatch);
		HANDLE_CURL_OPTION_CALLABLE(ch, CURLOPT_DEBUG, handlers.debug, curl_debug);

#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
		HANDLE_CURL_OPTION_CALLABLE(ch, CURLOPT_PREREQ, handlers.prereq, curl_prereqfunction);
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
		HANDLE_CURL_OPTION_CALLABLE(ch, CURLOPT_SSH_HOSTKEY, handlers.sshhostkey, curl_ssh_hostkeyfunction);
#endif

		/* Long options */
		case CURLOPT_SSL_VERIFYHOST:
			lval = zval_get_long(zvalue);
			if (lval == 1) {
				php_error_docref(NULL, E_NOTICE, "CURLOPT_SSL_VERIFYHOST no longer accepts the value 1, value 2 will be used instead");
				error = curl_easy_setopt(ch->cp, option, 2);
				break;
			}
			ZEND_FALLTHROUGH;
		case CURLOPT_AUTOREFERER:
		case CURLOPT_BUFFERSIZE:
		case CURLOPT_CONNECTTIMEOUT:
		case CURLOPT_COOKIESESSION:
		case CURLOPT_CRLF:
		case CURLOPT_DNS_CACHE_TIMEOUT:
		case CURLOPT_FAILONERROR:
		case CURLOPT_FILETIME:
		case CURLOPT_FORBID_REUSE:
		case CURLOPT_FRESH_CONNECT:
		case CURLOPT_FTP_USE_EPRT:
		case CURLOPT_FTP_USE_EPSV:
		case CURLOPT_HEADER:
		case CURLOPT_HTTPGET:
		case CURLOPT_HTTPPROXYTUNNEL:
		case CURLOPT_HTTP_VERSION:
		case CURLOPT_INFILESIZE:
		case CURLOPT_LOW_SPEED_LIMIT:
		case CURLOPT_LOW_SPEED_TIME:
		case CURLOPT_MAXCONNECTS:
		case CURLOPT_MAXREDIRS:
		case CURLOPT_NETRC:
		case CURLOPT_NOBODY:
		case CURLOPT_NOPROGRESS:
		case CURLOPT_NOSIGNAL:
		case CURLOPT_PORT:
		case CURLOPT_POST:
		case CURLOPT_PROXYPORT:
		case CURLOPT_PROXYTYPE:
		case CURLOPT_PUT:
		case CURLOPT_RESUME_FROM:
		case CURLOPT_SSLVERSION:
		case CURLOPT_SSL_VERIFYPEER:
		case CURLOPT_TIMECONDITION:
		case CURLOPT_TIMEOUT:
		case CURLOPT_TIMEVALUE:
		case CURLOPT_TRANSFERTEXT:
		case CURLOPT_UNRESTRICTED_AUTH:
		case CURLOPT_UPLOAD:
		case CURLOPT_VERBOSE:
		case CURLOPT_HTTPAUTH:
		case CURLOPT_FTP_CREATE_MISSING_DIRS:
		case CURLOPT_PROXYAUTH:
		case CURLOPT_SERVER_RESPONSE_TIMEOUT:
		case CURLOPT_IPRESOLVE:
		case CURLOPT_MAXFILESIZE:
		case CURLOPT_TCP_NODELAY:
		case CURLOPT_FTPSSLAUTH:
		case CURLOPT_IGNORE_CONTENT_LENGTH:
		case CURLOPT_FTP_SKIP_PASV_IP:
		case CURLOPT_FTP_FILEMETHOD:
		case CURLOPT_CONNECT_ONLY:
		case CURLOPT_LOCALPORT:
		case CURLOPT_LOCALPORTRANGE:
		case CURLOPT_SSL_SESSIONID_CACHE:
		case CURLOPT_FTP_SSL_CCC:
		case CURLOPT_SSH_AUTH_TYPES:
		case CURLOPT_CONNECTTIMEOUT_MS:
		case CURLOPT_HTTP_CONTENT_DECODING:
		case CURLOPT_HTTP_TRANSFER_DECODING:
		case CURLOPT_TIMEOUT_MS:
		case CURLOPT_NEW_DIRECTORY_PERMS:
		case CURLOPT_NEW_FILE_PERMS:
		case CURLOPT_USE_SSL:
		case CURLOPT_APPEND:
		case CURLOPT_DIRLISTONLY:
		case CURLOPT_PROXY_TRANSFER_MODE:
		case CURLOPT_ADDRESS_SCOPE:
		case CURLOPT_CERTINFO:
		case CURLOPT_PROTOCOLS:
		case CURLOPT_REDIR_PROTOCOLS:
		case CURLOPT_SOCKS5_GSSAPI_NEC:
		case CURLOPT_TFTP_BLKSIZE:
		case CURLOPT_FTP_USE_PRET:
		case CURLOPT_RTSP_CLIENT_CSEQ:
		case CURLOPT_RTSP_REQUEST:
		case CURLOPT_RTSP_SERVER_CSEQ:
		case CURLOPT_WILDCARDMATCH:
		case CURLOPT_GSSAPI_DELEGATION:
		case CURLOPT_ACCEPTTIMEOUT_MS:
		case CURLOPT_SSL_OPTIONS:
		case CURLOPT_TCP_KEEPALIVE:
		case CURLOPT_TCP_KEEPIDLE:
		case CURLOPT_TCP_KEEPINTVL:
		case CURLOPT_SASL_IR:
		case CURLOPT_EXPECT_100_TIMEOUT_MS:
		case CURLOPT_SSL_ENABLE_ALPN:
		case CURLOPT_SSL_ENABLE_NPN:
		case CURLOPT_HEADEROPT:
		case CURLOPT_SSL_VERIFYSTATUS:
		case CURLOPT_PATH_AS_IS:
		case CURLOPT_SSL_FALSESTART:
		case CURLOPT_PIPEWAIT:
		case CURLOPT_STREAM_WEIGHT:
		case CURLOPT_TFTP_NO_OPTIONS:
		case CURLOPT_TCP_FASTOPEN:
		case CURLOPT_KEEP_SENDING_ON_ERROR:
		case CURLOPT_PROXY_SSL_OPTIONS:
		case CURLOPT_PROXY_SSL_VERIFYHOST:
		case CURLOPT_PROXY_SSL_VERIFYPEER:
		case CURLOPT_PROXY_SSLVERSION:
		case CURLOPT_SUPPRESS_CONNECT_HEADERS:
		case CURLOPT_SOCKS5_AUTH:
		case CURLOPT_SSH_COMPRESSION:
		case CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS:
		case CURLOPT_DNS_SHUFFLE_ADDRESSES:
		case CURLOPT_HAPROXYPROTOCOL:
		case CURLOPT_DISALLOW_USERNAME_IN_URL:
#if LIBCURL_VERSION_NUM >= 0x073E00 /* Available since 7.62.0 */
		case CURLOPT_UPKEEP_INTERVAL_MS:
		case CURLOPT_UPLOAD_BUFFERSIZE:
#endif
#if LIBCURL_VERSION_NUM >= 0x074000 /* Available since 7.64.0 */
		case CURLOPT_HTTP09_ALLOWED:
#endif
#if LIBCURL_VERSION_NUM >= 0x074001 /* Available since 7.64.1 */
		case CURLOPT_ALTSVC_CTRL:
#endif
#if LIBCURL_VERSION_NUM >= 0x074100 /* Available since 7.65.0 */
		case CURLOPT_MAXAGE_CONN:
#endif
#if LIBCURL_VERSION_NUM >= 0x074500 /* Available since 7.69.0 */
		case CURLOPT_MAIL_RCPT_ALLLOWFAILS:
#endif
#if LIBCURL_VERSION_NUM >= 0x074a00 /* Available since 7.74.0 */
		case CURLOPT_HSTS_CTRL:
#endif
#if LIBCURL_VERSION_NUM >= 0x074c00 /* Available since 7.76.0 */
		case CURLOPT_DOH_SSL_VERIFYHOST:
		case CURLOPT_DOH_SSL_VERIFYPEER:
		case CURLOPT_DOH_SSL_VERIFYSTATUS:
#endif
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
		case CURLOPT_MAXLIFETIME_CONN:
#endif
#if LIBCURL_VERSION_NUM >= 0x075100 /* Available since 7.81.0 */
		case CURLOPT_MIME_OPTIONS:
#endif
#if LIBCURL_VERSION_NUM >= 0x075600 /* Available since 7.86.0 */
		case CURLOPT_WS_OPTIONS:
#endif
#if LIBCURL_VERSION_NUM >= 0x075700 /* Available since 7.87.0 */
		case CURLOPT_CA_CACHE_TIMEOUT:
		case CURLOPT_QUICK_EXIT:
#endif
#if LIBCURL_VERSION_NUM >= 0x080900 /* Available since 8.9.0 */
		case CURLOPT_TCP_KEEPCNT:
#endif
		case CURLOPT_FOLLOWLOCATION:
			lval = zval_get_long(zvalue);
			if ((option == CURLOPT_PROTOCOLS || option == CURLOPT_REDIR_PROTOCOLS) &&
				(PG(open_basedir) && *PG(open_basedir)) && (lval & CURLPROTO_FILE)) {
					php_error_docref(NULL, E_WARNING, "CURLPROTO_FILE cannot be activated when an open_basedir is set");
					return FAILURE;
			}
			error = curl_easy_setopt(ch->cp, option, lval);
			break;
		case CURLOPT_SAFE_UPLOAD:
			if (!zend_is_true(zvalue)) {
				zend_value_error("%s(): Disabling safe uploads is no longer supported", get_active_function_name());
				return FAILURE;
			}
			break;

		/* String options */
		case CURLOPT_CAINFO:
		case CURLOPT_CAPATH:
		case CURLOPT_COOKIE:
		case CURLOPT_EGDSOCKET:
		case CURLOPT_INTERFACE:
		case CURLOPT_PROXY:
		case CURLOPT_PROXYUSERPWD:
		case CURLOPT_REFERER:
		case CURLOPT_SSLCERTTYPE:
		case CURLOPT_SSLENGINE:
		case CURLOPT_SSLENGINE_DEFAULT:
		case CURLOPT_SSLKEY:
		case CURLOPT_SSLKEYPASSWD:
		case CURLOPT_SSLKEYTYPE:
		case CURLOPT_SSL_CIPHER_LIST:
		case CURLOPT_USERAGENT:
		case CURLOPT_COOKIELIST:
		case CURLOPT_FTP_ALTERNATIVE_TO_USER:
		case CURLOPT_SSH_HOST_PUBLIC_KEY_MD5:
		case CURLOPT_PROXYPASSWORD:
		case CURLOPT_PROXYUSERNAME:
		case CURLOPT_NOPROXY:
		case CURLOPT_SOCKS5_GSSAPI_SERVICE:
		case CURLOPT_MAIL_FROM:
		case CURLOPT_RTSP_STREAM_URI:
		case CURLOPT_RTSP_TRANSPORT:
		case CURLOPT_TLSAUTH_TYPE:
		case CURLOPT_TLSAUTH_PASSWORD:
		case CURLOPT_TLSAUTH_USERNAME:
		case CURLOPT_TRANSFER_ENCODING:
		case CURLOPT_DNS_SERVERS:
		case CURLOPT_MAIL_AUTH:
		case CURLOPT_LOGIN_OPTIONS:
		case CURLOPT_PINNEDPUBLICKEY:
		case CURLOPT_PROXY_SERVICE_NAME:
		case CURLOPT_SERVICE_NAME:
		case CURLOPT_DEFAULT_PROTOCOL:
		case CURLOPT_PRE_PROXY:
		case CURLOPT_PROXY_CAINFO:
		case CURLOPT_PROXY_CAPATH:
		case CURLOPT_PROXY_CRLFILE:
		case CURLOPT_PROXY_KEYPASSWD:
		case CURLOPT_PROXY_PINNEDPUBLICKEY:
		case CURLOPT_PROXY_SSL_CIPHER_LIST:
		case CURLOPT_PROXY_SSLCERT:
		case CURLOPT_PROXY_SSLCERTTYPE:
		case CURLOPT_PROXY_SSLKEY:
		case CURLOPT_PROXY_SSLKEYTYPE:
		case CURLOPT_PROXY_TLSAUTH_PASSWORD:
		case CURLOPT_PROXY_TLSAUTH_TYPE:
		case CURLOPT_PROXY_TLSAUTH_USERNAME:
		case CURLOPT_ABSTRACT_UNIX_SOCKET:
		case CURLOPT_REQUEST_TARGET:
		case CURLOPT_PROXY_TLS13_CIPHERS:
		case CURLOPT_TLS13_CIPHERS:
#if LIBCURL_VERSION_NUM >= 0x074001 /* Available since 7.64.1 */
		case CURLOPT_ALTSVC:
#endif
#if LIBCURL_VERSION_NUM >= 0x074200 /* Available since 7.66.0 */
		case CURLOPT_SASL_AUTHZID:
#endif
#if LIBCURL_VERSION_NUM >= 0x074700 /* Available since 7.71.0 */
		case CURLOPT_PROXY_ISSUERCERT:
#endif
#if LIBCURL_VERSION_NUM >= 0x074900 /* Available since 7.73.0 */
		case CURLOPT_SSL_EC_CURVES:
#endif
#if LIBCURL_VERSION_NUM >= 0x074b00 /* Available since 7.75.0 */
		case CURLOPT_AWS_SIGV4:
#endif
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
		case CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256:
#endif
#if LIBCURL_VERSION_NUM >= 0x075500 /* Available since 7.85.0 */
		case CURLOPT_PROTOCOLS_STR:
		case CURLOPT_REDIR_PROTOCOLS_STR:
#endif
		{
			zend_string *tmp_str;
			zend_string *str = zval_get_tmp_string(zvalue, &tmp_str);
#if LIBCURL_VERSION_NUM >= 0x075500 /* Available since 7.85.0 */
			if ((option == CURLOPT_PROTOCOLS_STR || option == CURLOPT_REDIR_PROTOCOLS_STR) &&
				(PG(open_basedir) && *PG(open_basedir))
					&& (php_memnistr(ZSTR_VAL(str), "file", sizeof("file") - 1, ZSTR_VAL(str) + ZSTR_LEN(str)) != NULL
					 || php_memnistr(ZSTR_VAL(str), "all", sizeof("all") - 1, ZSTR_VAL(str) + ZSTR_LEN(str)) != NULL)) {
					zend_tmp_string_release(tmp_str);
					php_error_docref(NULL, E_WARNING, "The FILE protocol cannot be activated when an open_basedir is set");
					return FAILURE;
			}
#endif
			zend_result ret = php_curl_option_str(ch, option, ZSTR_VAL(str), ZSTR_LEN(str));
			zend_tmp_string_release(tmp_str);
			return ret;
		}

		/* Curl nullable string options */
		case CURLOPT_CUSTOMREQUEST:
		case CURLOPT_FTPPORT:
		case CURLOPT_RANGE:
		case CURLOPT_FTP_ACCOUNT:
		case CURLOPT_RTSP_SESSION_ID:
		case CURLOPT_ACCEPT_ENCODING:
		case CURLOPT_DNS_INTERFACE:
		case CURLOPT_DNS_LOCAL_IP4:
		case CURLOPT_DNS_LOCAL_IP6:
		case CURLOPT_XOAUTH2_BEARER:
		case CURLOPT_UNIX_SOCKET_PATH:
#if LIBCURL_VERSION_NUM >= 0x073E00 /* Available since 7.62.0 */
		case CURLOPT_DOH_URL:
#endif
#if LIBCURL_VERSION_NUM >= 0x074a00 /* Available since 7.74.0 */
		case CURLOPT_HSTS:
#endif
		case CURLOPT_KRBLEVEL:
		// Authorization header would be implictly set
		// with an empty string thus we explictly set the option
		// to null to avoid this unwarranted side effect
		case CURLOPT_USERPWD:
		case CURLOPT_USERNAME:
		case CURLOPT_PASSWORD:
#if LIBCURL_VERSION_NUM >= 0x080e00 /* Available since 8.14.0 */
		case CURLOPT_SSL_SIGNATURE_ALGORITHMS:
#endif
		{
			if (Z_ISNULL_P(zvalue)) {
				error = curl_easy_setopt(ch->cp, option, NULL);
			} else {
				zend_string *tmp_str;
				zend_string *str = zval_get_tmp_string(zvalue, &tmp_str);
				zend_result ret = php_curl_option_str(ch, option, ZSTR_VAL(str), ZSTR_LEN(str));
				zend_tmp_string_release(tmp_str);
				return ret;
			}
			break;
		}

		/* Curl private option */
		case CURLOPT_PRIVATE:
		{
			zval_ptr_dtor(&ch->private_data);
			ZVAL_COPY(&ch->private_data, zvalue);
			return SUCCESS;
		}

		/* Curl url option */
		case CURLOPT_URL:
		{
			zend_string *tmp_str;
			zend_string *str = zval_get_tmp_string(zvalue, &tmp_str);
			zend_result ret = php_curl_option_url(ch, str);
			zend_tmp_string_release(tmp_str);
			return ret;
		}

		/* Curl file handle options */
		case CURLOPT_FILE:
		case CURLOPT_INFILE:
		case CURLOPT_STDERR:
		case CURLOPT_WRITEHEADER: {
			FILE *fp = NULL;
			php_stream *what = NULL;

			if (Z_TYPE_P(zvalue) != IS_NULL) {
				what = (php_stream *)zend_fetch_resource2_ex(zvalue, "File-Handle", php_file_le_stream(), php_file_le_pstream());
				if (!what) {
					return FAILURE;
				}

				if (FAILURE == php_stream_cast(what, PHP_STREAM_AS_STDIO, (void *) &fp, REPORT_ERRORS)) {
					return FAILURE;
				}

				if (!fp) {
					return FAILURE;
				}
			}

			error = CURLE_OK;
			switch (option) {
				case CURLOPT_FILE:
					if (!what) {
						if (!Z_ISUNDEF(ch->handlers.write->stream)) {
							zval_ptr_dtor(&ch->handlers.write->stream);
							ZVAL_UNDEF(&ch->handlers.write->stream);
						}
						ch->handlers.write->fp = NULL;
						ch->handlers.write->method = PHP_CURL_STDOUT;
					} else if (what->mode[0] != 'r' || what->mode[1] == '+') {
						zval_ptr_dtor(&ch->handlers.write->stream);
						ch->handlers.write->fp = fp;
						ch->handlers.write->method = PHP_CURL_FILE;
						ZVAL_COPY(&ch->handlers.write->stream, zvalue);
					} else {
						zend_value_error("%s(): The provided file handle must be writable", get_active_function_name());
						return FAILURE;
					}
					break;
				case CURLOPT_WRITEHEADER:
					if (!what) {
						if (!Z_ISUNDEF(ch->handlers.write_header->stream)) {
							zval_ptr_dtor(&ch->handlers.write_header->stream);
							ZVAL_UNDEF(&ch->handlers.write_header->stream);
						}
						ch->handlers.write_header->fp = NULL;
						ch->handlers.write_header->method = PHP_CURL_IGNORE;
					} else if (what->mode[0] != 'r' || what->mode[1] == '+') {
						zval_ptr_dtor(&ch->handlers.write_header->stream);
						ch->handlers.write_header->fp = fp;
						ch->handlers.write_header->method = PHP_CURL_FILE;
						ZVAL_COPY(&ch->handlers.write_header->stream, zvalue);
					} else {
						zend_value_error("%s(): The provided file handle must be writable", get_active_function_name());
						return FAILURE;
					}
					break;
				case CURLOPT_INFILE:
					if (!what) {
						if (!Z_ISUNDEF(ch->handlers.read->stream)) {
							zval_ptr_dtor(&ch->handlers.read->stream);
							ZVAL_UNDEF(&ch->handlers.read->stream);
						}
						ch->handlers.read->fp = NULL;
						ch->handlers.read->res = NULL;
					} else {
						zval_ptr_dtor(&ch->handlers.read->stream);
						ch->handlers.read->fp = fp;
						ch->handlers.read->res = Z_RES_P(zvalue);
						ZVAL_COPY(&ch->handlers.read->stream, zvalue);
					}
					break;
				case CURLOPT_STDERR:
					if (!what) {
						if (!Z_ISUNDEF(ch->handlers.std_err)) {
							zval_ptr_dtor(&ch->handlers.std_err);
							ZVAL_UNDEF(&ch->handlers.std_err);
						}
					} else if (what->mode[0] != 'r' || what->mode[1] == '+') {
						zval_ptr_dtor(&ch->handlers.std_err);
						ZVAL_COPY(&ch->handlers.std_err, zvalue);
					} else {
						zend_value_error("%s(): The provided file handle must be writable", get_active_function_name());
						return FAILURE;
					}
					ZEND_FALLTHROUGH;
				default:
					error = curl_easy_setopt(ch->cp, option, fp);
					break;
			}
			break;
		}

		/* Curl linked list options */
		case CURLOPT_HTTP200ALIASES:
		case CURLOPT_HTTPHEADER:
		case CURLOPT_POSTQUOTE:
		case CURLOPT_PREQUOTE:
		case CURLOPT_QUOTE:
		case CURLOPT_TELNETOPTIONS:
		case CURLOPT_MAIL_RCPT:
		case CURLOPT_RESOLVE:
		case CURLOPT_PROXYHEADER:
		case CURLOPT_CONNECT_TO:
		{
			zval *current;
			HashTable *ph;
			zend_string *val, *tmp_val;
			struct curl_slist *slist = NULL;

			if (Z_TYPE_P(zvalue) != IS_ARRAY) {
				const char *name = NULL;
				switch (option) {
					case CURLOPT_HTTPHEADER:
						name = "CURLOPT_HTTPHEADER";
						break;
					case CURLOPT_QUOTE:
						name = "CURLOPT_QUOTE";
						break;
					case CURLOPT_HTTP200ALIASES:
						name = "CURLOPT_HTTP200ALIASES";
						break;
					case CURLOPT_POSTQUOTE:
						name = "CURLOPT_POSTQUOTE";
						break;
					case CURLOPT_PREQUOTE:
						name = "CURLOPT_PREQUOTE";
						break;
					case CURLOPT_TELNETOPTIONS:
						name = "CURLOPT_TELNETOPTIONS";
						break;
					case CURLOPT_MAIL_RCPT:
						name = "CURLOPT_MAIL_RCPT";
						break;
					case CURLOPT_RESOLVE:
						name = "CURLOPT_RESOLVE";
						break;
					case CURLOPT_PROXYHEADER:
						name = "CURLOPT_PROXYHEADER";
						break;
					case CURLOPT_CONNECT_TO:
						name = "CURLOPT_CONNECT_TO";
						break;
				}

				zend_type_error("%s(): The %s option must have an array value", get_active_function_name(), name);
				return FAILURE;
			}

			ph = Z_ARRVAL_P(zvalue);
			ZEND_HASH_FOREACH_VAL(ph, current) {
				ZVAL_DEREF(current);
				val = zval_get_tmp_string(current, &tmp_val);
				struct curl_slist *new_slist = curl_slist_append(slist, ZSTR_VAL(val));
				zend_tmp_string_release(tmp_val);
				if (!new_slist) {
					curl_slist_free_all(slist);
					php_error_docref(NULL, E_WARNING, "Could not build curl_slist");
					return FAILURE;
				}
				slist = new_slist;
			} ZEND_HASH_FOREACH_END();

			if (slist) {
				if ((*ch->clone) == 1) {
					zend_hash_index_update_ptr(&ch->to_free->slist, option, slist);
				} else {
					zend_hash_next_index_insert_ptr(&ch->to_free->slist, slist);
				}
			}

			error = curl_easy_setopt(ch->cp, option, slist);

			break;
		}

		case CURLOPT_BINARYTRANSFER:
		case CURLOPT_DNS_USE_GLOBAL_CACHE:
			/* Do nothing, just backward compatibility */
			break;

		case CURLOPT_POSTFIELDS:
			if (Z_TYPE_P(zvalue) == IS_ARRAY) {
				if (zend_hash_num_elements(Z_ARRVAL_P(zvalue)) == 0) {
					/* no need to build the mime structure for empty hashtables;
					   also works around https://github.com/curl/curl/issues/6455 */
					curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDS, "");
					error = curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDSIZE, 0);
				} else {
					return build_mime_structure_from_hash(ch, zvalue);
				}
			} else {
				zend_string *tmp_str;
				zend_string *str = zval_get_tmp_string(zvalue, &tmp_str);
				/* with curl 7.17.0 and later, we can use COPYPOSTFIELDS, but we have to provide size before */
				error = curl_easy_setopt(ch->cp, CURLOPT_POSTFIELDSIZE, ZSTR_LEN(str));
				error = curl_easy_setopt(ch->cp, CURLOPT_COPYPOSTFIELDS, ZSTR_VAL(str));
				zend_tmp_string_release(tmp_str);
			}
			break;

		case CURLOPT_RETURNTRANSFER:
			if (zend_is_true(zvalue)) {
				ch->handlers.write->method = PHP_CURL_RETURN;
			} else {
				ch->handlers.write->method = PHP_CURL_STDOUT;
			}
			break;

		/* Curl off_t options */
		case CURLOPT_INFILESIZE_LARGE:
		case CURLOPT_MAX_RECV_SPEED_LARGE:
		case CURLOPT_MAX_SEND_SPEED_LARGE:
		case CURLOPT_MAXFILESIZE_LARGE:
		case CURLOPT_TIMEVALUE_LARGE:
			lval = zval_get_long(zvalue);
			error = curl_easy_setopt(ch->cp, option, (curl_off_t)lval);
			break;

		case CURLOPT_POSTREDIR:
			lval = zval_get_long(zvalue);
			error = curl_easy_setopt(ch->cp, CURLOPT_POSTREDIR, lval & CURL_REDIR_POST_ALL);
			break;

		/* the following options deal with files, therefore the open_basedir check
		 * is required.
		 */
		case CURLOPT_COOKIEFILE:
		case CURLOPT_COOKIEJAR:
		case CURLOPT_RANDOM_FILE:
		case CURLOPT_SSLCERT:
		case CURLOPT_NETRC_FILE:
		case CURLOPT_SSH_PRIVATE_KEYFILE:
		case CURLOPT_SSH_PUBLIC_KEYFILE:
		case CURLOPT_CRLFILE:
		case CURLOPT_ISSUERCERT:
		case CURLOPT_SSH_KNOWNHOSTS:
		{
			zend_string *tmp_str;
			zend_string *str = zval_get_tmp_string(zvalue, &tmp_str);
			zend_result ret;

			if (ZSTR_LEN(str) && php_check_open_basedir(ZSTR_VAL(str))) {
				zend_tmp_string_release(tmp_str);
				return FAILURE;
			}

			ret = php_curl_option_str(ch, option, ZSTR_VAL(str), ZSTR_LEN(str));
			zend_tmp_string_release(tmp_str);
			return ret;
		}

		case CURLINFO_HEADER_OUT:
			if (ZEND_FCC_INITIALIZED(ch->handlers.debug)) {
                zend_value_error("CURLINFO_HEADER_OUT option must not be set when the CURLOPT_DEBUGFUNCTION option is set");
                return FAILURE;
            }

			if (zend_is_true(zvalue)) {
				curl_easy_setopt(ch->cp, CURLOPT_DEBUGFUNCTION, curl_debug);
				curl_easy_setopt(ch->cp, CURLOPT_DEBUGDATA, (void *)ch);
				curl_easy_setopt(ch->cp, CURLOPT_VERBOSE, 1);
			} else {
				curl_easy_setopt(ch->cp, CURLOPT_DEBUGFUNCTION, NULL);
				curl_easy_setopt(ch->cp, CURLOPT_DEBUGDATA, NULL);
				curl_easy_setopt(ch->cp, CURLOPT_VERBOSE, 0);
			}
			break;

		case CURLOPT_SHARE:
			{
				if (Z_TYPE_P(zvalue) != IS_OBJECT) {
					break;
				}

				if (Z_OBJCE_P(zvalue) != curl_share_ce && Z_OBJCE_P(zvalue) != curl_share_persistent_ce) {
					break;
				}

				php_curlsh *sh = Z_CURL_SHARE_P(zvalue);

				curl_easy_setopt(ch->cp, CURLOPT_SHARE, sh->share);

				if (ch->share) {
					OBJ_RELEASE(&ch->share->std);
				}

				GC_ADDREF(&sh->std);
				ch->share = sh;
			}
			break;

		/* Curl blob options */
#if LIBCURL_VERSION_NUM >= 0x074700 /* Available since 7.71.0 */
		case CURLOPT_ISSUERCERT_BLOB:
		case CURLOPT_PROXY_ISSUERCERT_BLOB:
		case CURLOPT_PROXY_SSLCERT_BLOB:
		case CURLOPT_PROXY_SSLKEY_BLOB:
		case CURLOPT_SSLCERT_BLOB:
		case CURLOPT_SSLKEY_BLOB:
#if LIBCURL_VERSION_NUM >= 0x074d00 /* Available since 7.77.0 */
		case CURLOPT_CAINFO_BLOB:
		case CURLOPT_PROXY_CAINFO_BLOB:
#endif
			{
				zend_string *tmp_str;
				zend_string *str = zval_get_tmp_string(zvalue, &tmp_str);

				struct curl_blob stblob;
				stblob.data = ZSTR_VAL(str);
				stblob.len = ZSTR_LEN(str);
				stblob.flags = CURL_BLOB_COPY;
				error = curl_easy_setopt(ch->cp, option, &stblob);

				zend_tmp_string_release(tmp_str);
			}
			break;
#endif

		default:
			if (is_array_config) {
				zend_argument_value_error(2, "must contain only valid cURL options");
			} else {
				zend_argument_value_error(2, "is not a valid cURL option");
			}
			error = CURLE_UNKNOWN_OPTION;
			break;
	}

	SAVE_CURL_ERROR(ch, error);
	if (error != CURLE_OK) {
		return FAILURE;
	} else {
		return SUCCESS;
	}
}
/* }}} */

/* {{{ Set an option for a cURL transfer */
PHP_FUNCTION(curl_setopt)
{
	zval       *zid, *zvalue;
	zend_long        options;
	php_curl   *ch;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
		Z_PARAM_LONG(options)
		Z_PARAM_ZVAL(zvalue)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (_php_curl_setopt(ch, options, zvalue, 0) == SUCCESS) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ Set an array of option for a cURL transfer */
PHP_FUNCTION(curl_setopt_array)
{
	zval		*zid, *arr, *entry;
	php_curl	*ch;
	zend_ulong	option;
	zend_string	*string_key;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
		Z_PARAM_ARRAY(arr)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arr), option, string_key, entry) {
		if (UNEXPECTED(string_key)) {
			zend_argument_value_error(2, "contains an invalid cURL option");
			RETURN_THROWS();
		}

		ZVAL_DEREF(entry);
		if (_php_curl_setopt(ch, (zend_long) option, entry, 1) == FAILURE) {
			RETURN_FALSE;
		}
	} ZEND_HASH_FOREACH_END();

	RETURN_TRUE;
}
/* }}} */

/* {{{ _php_curl_cleanup_handle(ch)
   Cleanup an execution phase */
void _php_curl_cleanup_handle(php_curl *ch)
{
	smart_str_free(&ch->handlers.write->buf);
	if (ch->header.str) {
		zend_string_release_ex(ch->header.str, 0);
		ch->header.str = NULL;
	}

	memset(ch->err.str, 0, CURL_ERROR_SIZE + 1);
	ch->err.no = 0;
}
/* }}} */

/* {{{ Perform a cURL session */
PHP_FUNCTION(curl_exec)
{
	CURLcode	error;
	zval		*zid;
	php_curl	*ch;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	_php_curl_verify_handlers(ch, /* reporterror */ true);

	_php_curl_cleanup_handle(ch);

	error = curl_easy_perform(ch->cp);
	SAVE_CURL_ERROR(ch, error);

	if (error != CURLE_OK) {
		smart_str_free(&ch->handlers.write->buf);
		RETURN_FALSE;
	}

	if (!Z_ISUNDEF(ch->handlers.std_err)) {
		php_stream  *stream;
		stream = (php_stream*)zend_fetch_resource2_ex(&ch->handlers.std_err, NULL, php_file_le_stream(), php_file_le_pstream());
		if (stream) {
			php_stream_flush(stream);
		}
	}

	if (ch->handlers.write->method == PHP_CURL_RETURN && ch->handlers.write->buf.s) {
		smart_str_0(&ch->handlers.write->buf);
		RETURN_STR_COPY(ch->handlers.write->buf.s);
	}

	/* flush the file handle, so any remaining data is synched to disk */
	if (ch->handlers.write->method == PHP_CURL_FILE && ch->handlers.write->fp) {
		fflush(ch->handlers.write->fp);
	}
	if (ch->handlers.write_header->method == PHP_CURL_FILE && ch->handlers.write_header->fp) {
		fflush(ch->handlers.write_header->fp);
	}

	if (ch->handlers.write->method == PHP_CURL_RETURN) {
		RETURN_EMPTY_STRING();
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

/* {{{ Get information regarding a specific transfer */
PHP_FUNCTION(curl_getinfo)
{
	zval		*zid;
	php_curl	*ch;
	zend_long	option;
	bool option_is_null = 1;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG_OR_NULL(option, option_is_null)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (option_is_null) {
		char *s_code;
		/* libcurl expects long datatype. So far no cases are known where
		   it would be an issue. Using zend_long would truncate a 64-bit
		   var on Win64, so the exact long datatype fits everywhere, as
		   long as there's no 32-bit int overflow. */
		long l_code;
		double d_code;
		struct curl_certinfo *ci = NULL;
		zval listcode;
		curl_off_t co;

		array_init(return_value);

		if (curl_easy_getinfo(ch->cp, CURLINFO_EFFECTIVE_URL, &s_code) == CURLE_OK) {
			CAAS("url", s_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_CONTENT_TYPE, &s_code) == CURLE_OK) {
			if (s_code != NULL) {
				CAAS("content_type", s_code);
			} else {
				zval retnull;
				ZVAL_NULL(&retnull);
				CAAZ("content_type", &retnull);
			}
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_HTTP_CODE, &l_code) == CURLE_OK) {
			CAAL("http_code", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_HEADER_SIZE, &l_code) == CURLE_OK) {
			CAAL("header_size", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_REQUEST_SIZE, &l_code) == CURLE_OK) {
			CAAL("request_size", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_FILETIME, &l_code) == CURLE_OK) {
			CAAL("filetime", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_SSL_VERIFYRESULT, &l_code) == CURLE_OK) {
			CAAL("ssl_verify_result", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_REDIRECT_COUNT, &l_code) == CURLE_OK) {
			CAAL("redirect_count", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_TOTAL_TIME, &d_code) == CURLE_OK) {
			CAAD("total_time", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_NAMELOOKUP_TIME, &d_code) == CURLE_OK) {
			CAAD("namelookup_time", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_CONNECT_TIME, &d_code) == CURLE_OK) {
			CAAD("connect_time", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PRETRANSFER_TIME, &d_code) == CURLE_OK) {
			CAAD("pretransfer_time", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_SIZE_UPLOAD, &d_code) == CURLE_OK) {
			CAAD("size_upload", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_SIZE_DOWNLOAD, &d_code) == CURLE_OK) {
			CAAD("size_download", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_SPEED_DOWNLOAD, &d_code) == CURLE_OK) {
			CAAD("speed_download", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_SPEED_UPLOAD, &d_code) == CURLE_OK) {
			CAAD("speed_upload", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &d_code) == CURLE_OK) {
			CAAD("download_content_length", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_CONTENT_LENGTH_UPLOAD, &d_code) == CURLE_OK) {
			CAAD("upload_content_length", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_STARTTRANSFER_TIME, &d_code) == CURLE_OK) {
			CAAD("starttransfer_time", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_REDIRECT_TIME, &d_code) == CURLE_OK) {
			CAAD("redirect_time", d_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_REDIRECT_URL, &s_code) == CURLE_OK) {
			CAAS("redirect_url", s_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PRIMARY_IP, &s_code) == CURLE_OK) {
			CAAS("primary_ip", s_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_CERTINFO, &ci) == CURLE_OK) {
			array_init(&listcode);
			create_certinfo(ci, &listcode);
			CAAZ("certinfo", &listcode);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PRIMARY_PORT, &l_code) == CURLE_OK) {
			CAAL("primary_port", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_LOCAL_IP, &s_code) == CURLE_OK) {
			CAAS("local_ip", s_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_LOCAL_PORT, &l_code) == CURLE_OK) {
			CAAL("local_port", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_HTTP_VERSION, &l_code) == CURLE_OK) {
			CAAL("http_version", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PROTOCOL, &l_code) == CURLE_OK) {
			CAAL("protocol", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PROXY_SSL_VERIFYRESULT, &l_code) == CURLE_OK) {
			CAAL("ssl_verifyresult", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_SCHEME, &s_code) == CURLE_OK) {
			CAAS("scheme", s_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_APPCONNECT_TIME_T, &co) == CURLE_OK) {
			CAAL("appconnect_time_us", co);
		}
#if LIBCURL_VERSION_NUM >= 0x080600 /* Available since 8.6.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_QUEUE_TIME_T , &co) == CURLE_OK) {
			CAAL("queue_time_us", co);
		}
#endif
		if (curl_easy_getinfo(ch->cp, CURLINFO_CONNECT_TIME_T, &co) == CURLE_OK) {
			CAAL("connect_time_us", co);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_NAMELOOKUP_TIME_T, &co) == CURLE_OK) {
			CAAL("namelookup_time_us", co);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PRETRANSFER_TIME_T, &co) == CURLE_OK) {
			CAAL("pretransfer_time_us", co);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_REDIRECT_TIME_T, &co) == CURLE_OK) {
			CAAL("redirect_time_us", co);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_STARTTRANSFER_TIME_T, &co) == CURLE_OK) {
			CAAL("starttransfer_time_us", co);
		}
#if LIBCURL_VERSION_NUM >= 0x080a00 /* Available since 8.10.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_POSTTRANSFER_TIME_T, &co) == CURLE_OK) {
			CAAL("posttransfer_time_us", co);
		}
#endif
		if (curl_easy_getinfo(ch->cp, CURLINFO_TOTAL_TIME_T, &co) == CURLE_OK) {
			CAAL("total_time_us", co);
		}
		if (ch->header.str) {
			CAASTR("request_header", ch->header.str);
		}
#if LIBCURL_VERSION_NUM >= 0x074800 /* Available since 7.72.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_EFFECTIVE_METHOD, &s_code) == CURLE_OK) {
			CAAS("effective_method", s_code);
		}
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_CAPATH, &s_code) == CURLE_OK) {
			CAAS("capath", s_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_CAINFO, &s_code) == CURLE_OK) {
			CAAS("cainfo", s_code);
		}
#endif
#if LIBCURL_VERSION_NUM >= 0x080700 /* Available since 8.7.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_USED_PROXY, &l_code) == CURLE_OK) {
			CAAL("used_proxy", l_code);
		}
#endif
#if LIBCURL_VERSION_NUM >= 0x080c00 /* Available since 8.12.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_HTTPAUTH_USED, &l_code) == CURLE_OK) {
			CAAL("httpauth_used", l_code);
		}
		if (curl_easy_getinfo(ch->cp, CURLINFO_PROXYAUTH_USED, &l_code) == CURLE_OK) {
			CAAL("proxyauth_used", l_code);
		}
#endif
#if LIBCURL_VERSION_NUM >= 0x080200 /* Available since 8.2.0 */
		if (curl_easy_getinfo(ch->cp, CURLINFO_CONN_ID , &co) == CURLE_OK) {
			CAAL("conn_id", co);
		}
#endif
	} else {
		switch (option) {
			case CURLINFO_HEADER_OUT:
				if (ch->header.str) {
					RETURN_STR_COPY(ch->header.str);
				} else {
					RETURN_FALSE;
				}
			case CURLINFO_CERTINFO: {
				struct curl_certinfo *ci = NULL;

				array_init(return_value);

				if (curl_easy_getinfo(ch->cp, CURLINFO_CERTINFO, &ci) == CURLE_OK) {
					create_certinfo(ci, return_value);
				} else {
					RETURN_FALSE;
				}
				break;
			}
			case CURLINFO_PRIVATE:
				if (!Z_ISUNDEF(ch->private_data)) {
					RETURN_COPY(&ch->private_data);
				} else {
					RETURN_FALSE;
				}
				break;
			default: {
				int type = CURLINFO_TYPEMASK & option;
				switch (type) {
					case CURLINFO_STRING:
					{
						char *s_code = NULL;

						if (curl_easy_getinfo(ch->cp, option, &s_code) == CURLE_OK && s_code) {
							RETURN_STRING(s_code);
						} else {
							RETURN_FALSE;
						}
						break;
					}
					case CURLINFO_LONG:
					{
						zend_long code = 0;

						if (curl_easy_getinfo(ch->cp, option, &code) == CURLE_OK) {
							RETURN_LONG(code);
						} else {
							RETURN_FALSE;
						}
						break;
					}
					case CURLINFO_DOUBLE:
					{
						double code = 0.0;

						if (curl_easy_getinfo(ch->cp, option, &code) == CURLE_OK) {
							RETURN_DOUBLE(code);
						} else {
							RETURN_FALSE;
						}
						break;
					}
					case CURLINFO_SLIST:
					{
						struct curl_slist *slist;
						if (curl_easy_getinfo(ch->cp, option, &slist) == CURLE_OK) {
							struct curl_slist *current = slist;
							array_init(return_value);
							zend_hash_real_init_packed(Z_ARRVAL_P(return_value));
							while (current) {
								add_next_index_string(return_value, current->data);
								current = current->next;
							}
							curl_slist_free_all(slist);
						} else {
							RETURN_FALSE;
						}
						break;
					}
					case CURLINFO_OFF_T:
					{
						curl_off_t c_off;
						if (curl_easy_getinfo(ch->cp, option, &c_off) == CURLE_OK) {
							RETURN_LONG((long) c_off);
						} else {
							RETURN_FALSE;
						}
						break;
					}
					default:
						RETURN_FALSE;
				}
			}
		}
	}
}
/* }}} */

/* {{{ Return a string contain the last error for the current session */
PHP_FUNCTION(curl_error)
{
	zval		*zid;
	php_curl	*ch;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (ch->err.no) {
		ch->err.str[CURL_ERROR_SIZE] = 0;
		if (strlen(ch->err.str) > 0) {
			RETURN_STRING(ch->err.str);
		} else {
			RETURN_STRING(curl_easy_strerror(ch->err.no));
		}
	} else {
		RETURN_EMPTY_STRING();
	}
}
/* }}} */

/* {{{ Return an integer containing the last error number */
PHP_FUNCTION(curl_errno)
{
	zval		*zid;
	php_curl	*ch;

	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	RETURN_LONG(ch->err.no);
}
/* }}} */

/* {{{ Close a cURL session */
PHP_FUNCTION(curl_close)
{
	zval		*zid;
	php_curl	*ch;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (ch->in_callback) {
		zend_throw_error(NULL, "%s(): Attempt to close cURL handle from a callback", get_active_function_name());
		RETURN_THROWS();
	}
}
/* }}} */

static void curl_free_obj(zend_object *object)
{
	php_curl *ch = curl_from_obj(object);

#if PHP_CURL_DEBUG
	fprintf(stderr, "DTOR CALLED, ch = %x\n", ch);
#endif

	if (!ch->cp) {
		/* Can happen if constructor throws. */
		zend_object_std_dtor(&ch->std);
		return;
	}

	_php_curl_verify_handlers(ch, /* reporterror */ false);

	curl_easy_cleanup(ch->cp);

	/* cURL destructors should be invoked only by last curl handle */
	if (--(*ch->clone) == 0) {
		zend_llist_clean(&ch->to_free->post);
		zend_llist_clean(&ch->to_free->stream);

		zend_hash_destroy(&ch->to_free->slist);
		efree(ch->to_free);
		efree(ch->clone);
	}

	smart_str_free(&ch->handlers.write->buf);
	if (ZEND_FCC_INITIALIZED(ch->handlers.write->fcc)) {
		zend_fcc_dtor(&ch->handlers.write->fcc);
	}
	if (ZEND_FCC_INITIALIZED(ch->handlers.write_header->fcc)) {
		zend_fcc_dtor(&ch->handlers.write_header->fcc);
	}
	if (ZEND_FCC_INITIALIZED(ch->handlers.read->fcc)) {
		zend_fcc_dtor(&ch->handlers.read->fcc);
	}
	zval_ptr_dtor(&ch->handlers.std_err);
	if (ch->header.str) {
		zend_string_release_ex(ch->header.str, 0);
	}

	zval_ptr_dtor(&ch->handlers.write_header->stream);
	zval_ptr_dtor(&ch->handlers.write->stream);
	zval_ptr_dtor(&ch->handlers.read->stream);

	efree(ch->handlers.write);
	efree(ch->handlers.write_header);
	efree(ch->handlers.read);

	if (ZEND_FCC_INITIALIZED(ch->handlers.progress)) {
		zend_fcc_dtor(&ch->handlers.progress);
	}
	if (ZEND_FCC_INITIALIZED(ch->handlers.xferinfo)) {
		zend_fcc_dtor(&ch->handlers.xferinfo);
	}
	if (ZEND_FCC_INITIALIZED(ch->handlers.fnmatch)) {
		zend_fcc_dtor(&ch->handlers.fnmatch);
	}
	if (ZEND_FCC_INITIALIZED(ch->handlers.debug)) {
		zend_fcc_dtor(&ch->handlers.debug);
	}
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
	if (ZEND_FCC_INITIALIZED(ch->handlers.prereq)) {
		zend_fcc_dtor(&ch->handlers.prereq);
	}
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
	if (ZEND_FCC_INITIALIZED(ch->handlers.sshhostkey)) {
		zend_fcc_dtor(&ch->handlers.sshhostkey);
	}
#endif

	zval_ptr_dtor(&ch->postfields);
	zval_ptr_dtor(&ch->private_data);

	if (ch->share) {
		OBJ_RELEASE(&ch->share->std);
	}

	zend_object_std_dtor(&ch->std);
}
/* }}} */

/* {{{ return string describing error code */
PHP_FUNCTION(curl_strerror)
{
	zend_long code;
	const char *str;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(code)
	ZEND_PARSE_PARAMETERS_END();

	str = curl_easy_strerror(code);
	if (str) {
		RETURN_STRING(str);
	} else {
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ _php_curl_reset_handlers()
   Reset all handlers of a given php_curl */
static void _php_curl_reset_handlers(php_curl *ch)
{
	if (!Z_ISUNDEF(ch->handlers.write->stream)) {
		zval_ptr_dtor(&ch->handlers.write->stream);
		ZVAL_UNDEF(&ch->handlers.write->stream);
	}
	ch->handlers.write->fp = NULL;
	ch->handlers.write->method = PHP_CURL_STDOUT;

	if (!Z_ISUNDEF(ch->handlers.write_header->stream)) {
		zval_ptr_dtor(&ch->handlers.write_header->stream);
		ZVAL_UNDEF(&ch->handlers.write_header->stream);
	}
	ch->handlers.write_header->fp = NULL;
	ch->handlers.write_header->method = PHP_CURL_IGNORE;

	if (!Z_ISUNDEF(ch->handlers.read->stream)) {
		zval_ptr_dtor(&ch->handlers.read->stream);
		ZVAL_UNDEF(&ch->handlers.read->stream);
	}
	ch->handlers.read->fp = NULL;
	ch->handlers.read->res = NULL;
	ch->handlers.read->method  = PHP_CURL_DIRECT;

	if (!Z_ISUNDEF(ch->handlers.std_err)) {
		zval_ptr_dtor(&ch->handlers.std_err);
		ZVAL_UNDEF(&ch->handlers.std_err);
	}

	if (ZEND_FCC_INITIALIZED(ch->handlers.progress)) {
		zend_fcc_dtor(&ch->handlers.progress);
	}

	if (ZEND_FCC_INITIALIZED(ch->handlers.xferinfo)) {
		zend_fcc_dtor(&ch->handlers.xferinfo);
	}

	if (ZEND_FCC_INITIALIZED(ch->handlers.fnmatch)) {
		zend_fcc_dtor(&ch->handlers.fnmatch);
	}

	if (ZEND_FCC_INITIALIZED(ch->handlers.debug)) {
		zend_fcc_dtor(&ch->handlers.debug);
	}
#if LIBCURL_VERSION_NUM >= 0x075000 /* Available since 7.80.0 */
	if (ZEND_FCC_INITIALIZED(ch->handlers.prereq)) {
		zend_fcc_dtor(&ch->handlers.prereq);
	}
#endif
#if LIBCURL_VERSION_NUM >= 0x075400 /* Available since 7.84.0 */
	if (ZEND_FCC_INITIALIZED(ch->handlers.sshhostkey)) {
		zend_fcc_dtor(&ch->handlers.sshhostkey);
	}
#endif
}
/* }}} */

/* {{{ Reset all options of a libcurl session handle */
PHP_FUNCTION(curl_reset)
{
	zval       *zid;
	php_curl   *ch;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (ch->in_callback) {
		zend_throw_error(NULL, "%s(): Attempt to reset cURL handle from a callback", get_active_function_name());
		RETURN_THROWS();
	}

	curl_easy_reset(ch->cp);
	_php_curl_reset_handlers(ch);
	_php_curl_set_default_options(ch);
}
/* }}} */

/* {{{ URL encodes the given string */
PHP_FUNCTION(curl_escape)
{
	zend_string *str;
	char        *res;
	zval        *zid;
	php_curl    *ch;

	ZEND_PARSE_PARAMETERS_START(2,2)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (ZEND_SIZE_T_INT_OVFL(ZSTR_LEN(str))) {
		RETURN_FALSE;
	}

	if ((res = curl_easy_escape(ch->cp, ZSTR_VAL(str), ZSTR_LEN(str)))) {
		RETVAL_STRING(res);
		curl_free(res);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ URL decodes the given string */
PHP_FUNCTION(curl_unescape)
{
	char        *out = NULL;
	int          out_len;
	zval        *zid;
	zend_string *str;
	php_curl    *ch;

	ZEND_PARSE_PARAMETERS_START(2,2)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
		Z_PARAM_STR(str)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	if (ZEND_SIZE_T_INT_OVFL(ZSTR_LEN(str))) {
		RETURN_FALSE;
	}

	if ((out = curl_easy_unescape(ch->cp, ZSTR_VAL(str), ZSTR_LEN(str), &out_len))) {
		RETVAL_STRINGL(out, out_len);
		curl_free(out);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ pause and unpause a connection */
PHP_FUNCTION(curl_pause)
{
	zend_long       bitmask;
	zval       *zid;
	php_curl   *ch;

	ZEND_PARSE_PARAMETERS_START(2,2)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
		Z_PARAM_LONG(bitmask)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	RETURN_LONG(curl_easy_pause(ch->cp, bitmask));
}
/* }}} */

#if LIBCURL_VERSION_NUM >= 0x073E00 /* Available since 7.62.0 */
/* {{{ perform connection upkeep checks */
PHP_FUNCTION(curl_upkeep)
{
	CURLcode	error;
	zval		*zid;
	php_curl	*ch;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT_OF_CLASS(zid, curl_ce)
	ZEND_PARSE_PARAMETERS_END();

	ch = Z_CURL_P(zid);

	error = curl_easy_upkeep(ch->cp);
	SAVE_CURL_ERROR(ch, error);

	RETURN_BOOL(error == CURLE_OK);
}
/*}}} */
#endif
