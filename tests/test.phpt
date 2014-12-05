--TEST--
raphf test
--SKIPIF--
<?php
if (!extension_loaded("raphf")) {
	die("skip need ext/raphf");
}
if (!defined("RAPHF_TEST")) {
	die("skip need RAPHF_TEST defined (-DPHP_RAPHF_TEST=1)");
}
?>
--INI--
raphf.persistent_handle.limit=0
--FILE--
<?php

function dumper($id) {
	return function() use ($id) {
		echo "### back '$id':\n";
		for ($i=0; $i<func_num_args(); ++$i) {
			echo "#### arg $i: ";
			var_dump(func_get_arg($i));
		}
		/* relay arguments back */
		return func_get_args();
	};
}

echo "## call provide:\n";
var_dump(raphf\provide("test",dumper("ctor"),dumper("copy"),dumper("dtor"),"data value",dumper("data_dtor")));

echo "## call concede:\n";
var_dump($rf = raphf\concede("test","1"));

echo "## call handle_ctor:\n";
var_dump($h = raphf\handle_ctor($rf, 1));

echo "## call handle_copy:\n";
var_dump($h2 = raphf\handle_copy($rf, $h));
var_dump(raphf\stat_persistent_handles());

echo "## call handle_dtor:\n";
var_dump(raphf\handle_dtor($rf, $h));
var_dump(raphf\stat_persistent_handles());

echo "## call handle_dtor:\n";
var_dump(raphf\handle_dtor($rf, $h2));
var_dump(raphf\stat_persistent_handles());

echo "## cleanup:\n";
var_dump(raphf\dispute($rf), $rf);
var_dump(raphf\conceal("test"));
var_dump(raphf\stat_persistent_handles());

?>
--EXPECTF--
## call provide:
bool(true)
## call concede:
resource(4) of type (raphf_user)
## call handle_ctor:
### back 'ctor':
#### arg 0: string(10) "data value"
#### arg 1: int(1)
array(2) {
  [0]=>
  string(10) "data value"
  [1]=>
  int(1)
}
## call handle_copy:
### back 'copy':
#### arg 0: string(10) "data value"
#### arg 1: array(2) {
  [0]=>
  string(10) "data value"
  [1]=>
  int(1)
}
array(2) {
  [0]=>
  string(10) "data value"
  [1]=>
  array(2) {
    [0]=>
    string(10) "data value"
    [1]=>
    int(1)
  }
}
object(stdClass)#%d (1) {
  ["test"]=>
  array(1) {
    [1]=>
    array(2) {
      ["used"]=>
      int(2)
      ["free"]=>
      int(0)
    }
  }
}
## call handle_dtor:
### back 'dtor':
#### arg 0: string(10) "data value"
#### arg 1: array(2) {
  [0]=>
  string(10) "data value"
  [1]=>
  int(1)
}
NULL
object(stdClass)#%d (1) {
  ["test"]=>
  array(1) {
    [1]=>
    array(2) {
      ["used"]=>
      int(1)
      ["free"]=>
      int(0)
    }
  }
}
## call handle_dtor:
### back 'dtor':
#### arg 0: string(10) "data value"
#### arg 1: array(2) {
  [0]=>
  string(10) "data value"
  [1]=>
  array(2) {
    [0]=>
    string(10) "data value"
    [1]=>
    int(1)
  }
}
NULL
object(stdClass)#%d (1) {
  ["test"]=>
  array(1) {
    [1]=>
    array(2) {
      ["used"]=>
      int(0)
      ["free"]=>
      int(0)
    }
  }
}
## cleanup:
bool(true)
resource(4) of type (Unknown)
### back 'data_dtor':
#### arg 0: string(10) "data value"
bool(true)
bool(false)
