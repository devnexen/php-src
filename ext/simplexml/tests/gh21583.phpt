--TEST--
GH-21583 (SimpleXML empty element incorrectly casts to true)
--EXTENSIONS--
simplexml
--FILE--
<?php
$xml = new SimpleXMLElement('<foo><bar/><baz attr="val"/><qux>text</qux></foo>');

var_dump((bool) $xml->bar);
var_dump((bool) $xml->baz);
var_dump((bool) $xml->qux);
var_dump((bool) $xml->nonexistent);
?>
--EXPECT--
bool(false)
bool(true)
bool(true)
bool(false)
