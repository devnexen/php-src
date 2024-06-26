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
   | Authors: Christian Stocker <chregu@php.net>                          |
   |          Rob Richards <rrichards@php.net>                            |
   +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "php.h"
#if defined(HAVE_LIBXML) && defined(HAVE_DOM)
#include "php_dom.h"

/*
* class DOMCdataSection extends DOMText
*
* URL: https://www.w3.org/TR/2003/WD-DOM-Level-3-Core-20030226/DOM3-Core.html#ID-667469212
* Since:
*/

/* {{{ */
PHP_METHOD(DOMCdataSection, __construct)
{
	xmlNodePtr nodep = NULL, oldnode = NULL;
	dom_object *intern;
	char *value = NULL;
	size_t value_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &value, &value_len) == FAILURE) {
		RETURN_THROWS();
	}

	nodep = xmlNewCDataBlock(NULL, BAD_CAST value, value_len);

	if (!nodep) {
		php_dom_throw_error(INVALID_STATE_ERR, true);
		RETURN_THROWS();
	}

	intern = Z_DOMOBJ_P(ZEND_THIS);
	oldnode = dom_object_get_node(intern);
	if (oldnode != NULL) {
		php_libxml_node_decrement_resource((php_libxml_node_object *)intern);
	}
	php_libxml_increment_node_ptr((php_libxml_node_object *)intern, nodep, (void *)intern);
}
/* }}} end DOMCdataSection::__construct */

#endif
