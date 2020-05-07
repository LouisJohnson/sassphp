--TEST--
check callback argument type for list input
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        return sass_make_string(implode('|', $in[0]));
    },
]);
echo $sass->compile('@debug a(Helvetica Arial sans-serif);');

?>
--EXPECT--
stdin:1 DEBUG: Helvetica|Arial|sans-serif
