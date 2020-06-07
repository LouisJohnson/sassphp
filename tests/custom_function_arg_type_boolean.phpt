--TEST--
check callback argument type for boolean input
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        return sass_make_string(gettype($in[0]));
    },
]);
echo $sass->compile('@debug a(true); @debug a(false);');

?>
--EXPECT--
stdin:1 DEBUG: boolean
stdin:1 DEBUG: boolean
