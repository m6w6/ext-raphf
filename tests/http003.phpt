--TEST--
pecl/http-v2 - clean with id only
--SKIPIF--
<?php
if (!extension_loaded("http")) {
	die("skip pecl/http needed");
}
if (!class_exists("http\\Client", false)) {
	die("skip pecl/http-v2 with curl support needed");
}
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Client("curl", "php.net:80");
do {
	$c->enqueue(new http\Client\Request("GET", "http://php.net"));
} while (count($c) < 3);

unset($c);

var_dump(raphf\stat_persistent_handles());

raphf\clean_persistent_handles(null, "php.net:80");

var_dump(raphf\stat_persistent_handles());

?>
Done
--EXPECTF--
Test
object(stdClass)#%d (2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(1)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(3)
    }
  }
}
object(stdClass)#%d (2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(0)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(0)
    }
  }
}
Done
