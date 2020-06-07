--TEST--
produce map from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a()' => function(){
        $retval = sass_make_map([
            sass_make_string('test'),
            'sample' => sass_make_boolean(false),
        ]);
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a();');

?>
--EXPECT--
stdin:1 DEBUG: (0: test, sample: false)
