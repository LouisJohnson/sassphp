--TEST--
produce number (with optional unit) from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a, $b)' => function($in){
        assert(gettype($in[0]) === 'double');
        $retval = sass_make_number(intval($in[0]), $in[1] ? 'px' : '');
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a(1.5, true); @debug a(2.5, false);');

?>
--EXPECT--
stdin:1 DEBUG: 1px
stdin:1 DEBUG: 2
