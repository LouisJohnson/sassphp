--TEST--
produce list from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a, $b)' => function($in){
        $retval = sass_make_list(array_map('sass_make_string', ['test', 'sample']), $in[0], $in[1]);
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a(" ", false); @debug a(",", true);');

?>
--EXPECT--
stdin:1 DEBUG: test sample
stdin:1 DEBUG: [test, sample]
