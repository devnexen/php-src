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
   | Author: Pierre A. Joye <pierre@php.net>                              |
   |         Gustavo Lopes  <cataphract@php.net>                          |
   +----------------------------------------------------------------------+
 */

/* {{{ includes */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <php.h>

#include <unicode/uidna.h>
#include <unicode/ustring.h>

#include "idn.h"

extern "C" {
#include "intl_error.h"
}
/* }}} */

enum {
	INTL_IDN_TO_ASCII = 0,
	INTL_IDN_TO_UTF8
};

/* like INTL_CHECK_STATUS, but as a function and varying the name of the func */
static zend_result php_intl_idn_check_status(UErrorCode err, const char *msg)
{
	intl_error_set_code(NULL, err);
	if (U_FAILURE(err)) {
		char *buff;
		spprintf(&buff, 0, "%s: %s",
			get_active_function_name(),
			msg);
		intl_error_set_custom_msg(NULL, buff, 1);
		efree(buff);
		return FAILURE;
	}

	return SUCCESS;
}

static void php_intl_idn_to_46(INTERNAL_FUNCTION_PARAMETERS,
		const zend_string *domain, uint32_t option, int mode, zval *idna_info)
{
	UErrorCode	  status = U_ZERO_ERROR;
	UIDNA		  *uts46;
	int32_t		  len;
	int32_t       buffer_capac;
	zend_string	  *buffer;
	UIDNAInfo	  info = UIDNA_INFO_INITIALIZER;

	uts46 = uidna_openUTS46(option, &status);
	if (php_intl_idn_check_status(status, "failed to open UIDNA instance") == FAILURE) {
		RETURN_FALSE;
	}

	if (mode == INTL_IDN_TO_ASCII) {
		buffer_capac = 255;
		buffer = zend_string_alloc(buffer_capac, 0);
		len = uidna_nameToASCII_UTF8(uts46, ZSTR_VAL(domain), ZSTR_LEN(domain),
				ZSTR_VAL(buffer), buffer_capac, &info, &status);
	} else {
		buffer_capac = 252*4;
		buffer = zend_string_alloc(buffer_capac, 0);
		len = uidna_nameToUnicodeUTF8(uts46, ZSTR_VAL(domain), ZSTR_LEN(domain),
				ZSTR_VAL(buffer), buffer_capac, &info, &status);
	}
	if (len >= buffer_capac || php_intl_idn_check_status(status, "failed to convert name") == FAILURE) {
		uidna_close(uts46);
		zend_string_efree(buffer);
		RETURN_FALSE;
	}

	ZSTR_VAL(buffer)[len] = '\0';
	ZSTR_LEN(buffer) = len;

	if (idna_info) {
		add_assoc_str_ex(idna_info, "result", sizeof("result")-1, zend_string_copy(buffer));
		add_assoc_bool_ex(idna_info, "isTransitionalDifferent",
				sizeof("isTransitionalDifferent")-1, info.isTransitionalDifferent);
		add_assoc_long_ex(idna_info, "errors", sizeof("errors")-1, (zend_long)info.errors);
	}

	if (info.errors == 0) {
		RETVAL_STR(buffer);
	} else {
		zend_string_release_ex(buffer, false);
		RETVAL_FALSE;
	}

	uidna_close(uts46);
}

static void php_intl_idn_handoff(INTERNAL_FUNCTION_PARAMETERS, int mode)
{
	zend_string *domain;
	zend_long option = UIDNA_DEFAULT,
	variant = INTL_IDN_VARIANT_UTS46;
	zval *idna_info = NULL;

	intl_error_reset(NULL);

	ZEND_PARSE_PARAMETERS_START(1, 4)
		Z_PARAM_STR(domain)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(option)
		Z_PARAM_LONG(variant)
		Z_PARAM_ZVAL(idna_info)
	ZEND_PARSE_PARAMETERS_END();

	if (ZSTR_LEN(domain) == 0) {
		zend_argument_must_not_be_empty_error(1);
		RETURN_THROWS();
	}
	if (ZSTR_LEN(domain) > INT32_MAX - 1) {
		zend_argument_value_error(1, "must be less than " PRId32 " bytes", INT32_MAX);
		RETURN_THROWS();
	}
	if (variant != INTL_IDN_VARIANT_UTS46) {
		zend_argument_value_error(2, "must be INTL_IDNA_VARIANT_UTS46");
		RETURN_THROWS();
	}
	/* don't check options; it wasn't checked before */

	if (idna_info != NULL) {
		idna_info = zend_try_array_init(idna_info);
		if (!idna_info) {
			RETURN_THROWS();
		}
	}

	php_intl_idn_to_46(INTERNAL_FUNCTION_PARAM_PASSTHRU, domain, (uint32_t)option, mode, idna_info);
}

/* {{{ Converts an Unicode domain to ASCII representation, as defined in the IDNA RFC */
U_CFUNC PHP_FUNCTION(idn_to_ascii)
{
	php_intl_idn_handoff(INTERNAL_FUNCTION_PARAM_PASSTHRU, INTL_IDN_TO_ASCII);
}
/* }}} */


/* {{{ Converts an ASCII representation of the domain to Unicode (UTF-8), as defined in the IDNA RFC */
U_CFUNC PHP_FUNCTION(idn_to_utf8)
{
	php_intl_idn_handoff(INTERNAL_FUNCTION_PARAM_PASSTHRU, INTL_IDN_TO_UTF8);
}
/* }}} */
