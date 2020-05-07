--TEST--
check callback argument type for number input
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        return sass_make_string(gettype($in[0]) . ',' . $in[0]);
    },
]);
echo $sass->compile('@debug a(10.5); @debug a(10.5px);');

?>
--EXPECT--
stdin:1 DEBUG: double,10.5
stdin:1 DEBUG: string,10.5px
