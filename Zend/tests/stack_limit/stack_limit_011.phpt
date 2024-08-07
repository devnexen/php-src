--TEST--
Stack limit 011 - Stack limit exhaustion during unwinding
--SKIPIF--
<?php
if (!function_exists('zend_test_zend_call_stack_get')) die("skip zend_test_zend_call_stack_get() is not available");
?>
--EXTENSIONS--
zend_test
--INI--
zend.max_allowed_stack_size=512K
--FILE--
<?php

var_dump(zend_test_zend_call_stack_get());

function replace2() {
    return preg_replace_callback('#.#', function () {
        replace2();
    }, 'x');
}
function replace() {
    static $once = false;
    return preg_replace_callback('#.#', function () use (&$once) {
        try {
            replace();
        } finally {
            if (!$once) {
                $once = true;
                replace2();
            }
        }
    }, 'x');
}

try {
    replace();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
    echo 'Previous: ', $e->getPrevious()->getMessage(), "\n";
}

?>
--EXPECTF--
array(4) {
  ["base"]=>
  string(%d) "0x%x"
  ["max_size"]=>
  string(%d) "0x%x"
  ["position"]=>
  string(%d) "0x%x"
  ["EG(stack_limit)"]=>
  string(%d) "0x%x"
}
Maximum call stack size of %d bytes (zend.max_allowed_stack_size - zend.reserved_stack_size) reached. Infinite recursion?
Previous: Maximum call stack size of %d bytes (zend.max_allowed_stack_size - zend.reserved_stack_size) reached. Infinite recursion?
