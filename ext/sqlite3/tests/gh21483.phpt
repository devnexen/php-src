--TEST--
GH-21483 (SQLite3 free list comparator uses correct pointer types)
--EXTENSIONS--
sqlite3
--FILE--
<?php

$db = new SQLite3(':memory:');
$db->exec('CREATE TABLE test (id INTEGER PRIMARY KEY, val TEXT)');
$db->exec("INSERT INTO test VALUES (1, 'a')");
$db->exec("INSERT INTO test VALUES (2, 'b')");
$db->exec("INSERT INTO test VALUES (3, 'c')");

$r1 = $db->query("SELECT * FROM test WHERE id = 1");
$r2 = $db->query("SELECT * FROM test WHERE id = 2");
$r3 = $db->query("SELECT * FROM test WHERE id = 3");

var_dump($r1->fetchArray(SQLITE3_ASSOC));
var_dump($r2->fetchArray(SQLITE3_ASSOC));
var_dump($r3->fetchArray(SQLITE3_ASSOC));

$r2->finalize();
$r1->finalize();
$r3->finalize();

$stmt = $db->prepare("SELECT * FROM test WHERE id = ?");
$stmt->bindValue(1, 2, SQLITE3_INTEGER);
$result = $stmt->execute();
var_dump($result->fetchArray(SQLITE3_ASSOC));
$result->finalize();
$stmt->close();

$db->close();
echo "Done\n";
?>
--EXPECT--
array(2) {
  ["id"]=>
  int(1)
  ["val"]=>
  string(1) "a"
}
array(2) {
  ["id"]=>
  int(2)
  ["val"]=>
  string(1) "b"
}
array(2) {
  ["id"]=>
  int(3)
  ["val"]=>
  string(1) "c"
}
array(2) {
  ["id"]=>
  int(2)
  ["val"]=>
  string(1) "b"
}
Done
