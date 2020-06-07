--TEST--
produce boolean from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        assert(gettype($in[0]) === 'boolean');
        $retval = sass_make_boolean($in[0]);
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a(true); @debug a(false);');

?>
--EXPECT--
stdin:1 DEBUG: true
stdin:1 DEBUG: false
