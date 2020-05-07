--TEST--
produce string from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        assert(gettype($in[0]) === 'double');
        $retval = sass_make_string($in[0]);
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a(1.5);');

?>
--EXPECT--
stdin:1 DEBUG: 1.5
