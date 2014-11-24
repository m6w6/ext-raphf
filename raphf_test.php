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

var_dump(raphf\dispute($rf), $rf);

