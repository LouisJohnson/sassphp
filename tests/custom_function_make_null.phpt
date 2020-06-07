--TEST--
produce null from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a()' => function(){
        $retval = sass_make_null();
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a();');

?>
--EXPECT--
stdin:1 DEBUG: null
