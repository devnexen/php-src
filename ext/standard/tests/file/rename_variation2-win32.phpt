--TEST--
Test rename() function: usage variations
--SKIPIF--
<?php
if (substr(PHP_OS, 0, 3) != 'WIN') {
    die('skip.. only for Windows');
}
?>
--FILE--
<?php
require __DIR__.'/file.inc';

$file_path = __DIR__;
mkdir("$file_path/rename_variation2-win32_dir");

/* Renaming a file and directory to numeric name */
echo "\n*** Testing rename() by renaming a file and directory to numeric name ***\n";
$fp = fopen($file_path."/rename_variation2-win32.tmp", "w");
fclose($fp);

// renaming existing file to numeric name
var_dump( rename($file_path."/rename_variation2-win32.tmp", $file_path."/12346") );

// ensure that rename worked fine
var_dump( file_exists($file_path."/rename_variation2-win32.tmp" ) );  // expecting false
var_dump( file_exists($file_path."/12346" ) );  // expecting true

unlink($file_path."/12346");

// renaming a directory to numeric name
var_dump( rename($file_path."/rename_variation2-win32_dir/", $file_path."/12346") );

// ensure that rename worked fine
var_dump( file_exists($file_path."/rename_variation2-win32_dir" ) );  // expecting false
var_dump( file_exists($file_path."/12346" ) );  // expecting true

echo "Done\n";
?>
--CLEAN--
<?php
$file_path = __DIR__;
rmdir($file_path."/12346");
?>
--EXPECT--
*** Testing rename() by renaming a file and directory to numeric name ***
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
Done
