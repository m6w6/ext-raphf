--TEST--
pecl/http-v2 - partial clean
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

$h = (array) raphf\stat_persistent_handles();
var_dump(array_intersect_key($h, array_flip(preg_grep("/^http/", array_keys($h)))));

$c = new http\Client("curl", "php.net:80");
$c2 = new http\Client("curl", "php.net:80");
do {
	$c->enqueue(new http\Client\Request("GET", "http://php.net"));
	$c2->enqueue(new http\Client\Request("GET", "http://php.net"));
} while (count($c) < 3);

$h = (array) raphf\stat_persistent_handles();
var_dump(array_intersect_key($h, array_flip(preg_grep("/^http/", array_keys($h)))));

unset($c);

$h = (array) raphf\stat_persistent_handles();
var_dump(array_intersect_key($h, array_flip(preg_grep("/^http/", array_keys($h)))));

raphf\clean_persistent_handles();

$h = (array) raphf\stat_persistent_handles();
var_dump(array_intersect_key($h, array_flip(preg_grep("/^http/", array_keys($h)))));

?>
Done
--EXPECTF--
Test
array(2) {
  ["http\Client\Curl"]=>
  array(0) {
  }
  ["http\Client\Curl\Request"]=>
  array(0) {
  }
}
array(2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(2)
      ["free"]=>
      int(0)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(6)
      ["free"]=>
      int(0)
    }
  }
}
array(2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(1)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(3)
      ["free"]=>
      int(3)
    }
  }
}
array(2) {
  ["http\Client\Curl"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
  ["http\Client\Curl\Request"]=>
  array(1) {
    ["php.net:80"]=>
    array(2) {
      ["used"]=>
      int(3)
      ["free"]=>
      int(0)
    }
  }
}
Done
