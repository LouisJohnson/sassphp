--TEST--
check callback argument type for map input
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        $ktype = implode(',', array_map('gettype', array_keys($in[0])));
        $vtype = implode(',', array_map('gettype', $in[0]));
        $value = implode(',', array_map('strval', $in[0]));
        return sass_make_string($ktype . ',' . $vtype . ',' . $value);
    },
]);
echo $sass->compile('@debug a((0: true, 1px: false, test: blue));');

?>
--EXPECT--
stdin:1 DEBUG: integer,string,string,boolean,boolean,string,1,,#0000ff
