--TEST--
check callback argument type for string input
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        return sass_make_string($in[0]);
    },
]);
echo $sass->compile('@debug a(sample); @debug a(\'sample2\'); @debug a("sample3");');

?>
--EXPECT--
stdin:1 DEBUG: sample
stdin:1 DEBUG: sample2
stdin:1 DEBUG: sample3
