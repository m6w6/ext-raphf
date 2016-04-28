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

$c = new http\Client("curl", "PHP");
do {
	$c->enqueue(new http\Client\Request("GET", "http://php.net"));
} while (count($c) < 3);

unset($c);

$h = (array) raphf\stat_persistent_handles();
var_dump(array_intersect_key($h, array_flip(preg_grep("/^http/", array_keys($h)))));

raphf\clean_persistent_handles(null, "PHP");
raphf\clean_persistent_handles(null, "PHP:php.net:80");

$h = (array) raphf\stat_persistent_handles();
var_dump(array_intersect_key($h, array_flip(preg_grep("/^http/", array_keys($h)))));

?>
Done
--EXPECTF--
Test
array(2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["PHP"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(1)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["PHP:php.net:80"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(3)
    }
  }
}
array(2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["PHP"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(0)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["PHP:php.net:80"]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(0)
    }
  }
}
Done
