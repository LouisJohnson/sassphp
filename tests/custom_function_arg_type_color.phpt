--TEST--
check callback argument type for color input
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
echo $sass->compile('@debug a(black); @debug a(rgba(128,64,192,0.5));');

?>
--EXPECT--
stdin:1 DEBUG: string,#000000
stdin:1 DEBUG: string,#8040c07f
