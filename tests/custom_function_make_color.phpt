--TEST--
produce color from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a($a)' => function($in){
        $retval = sass_make_color(10, 20, 30, $in[0] ? 0.4 : 1);
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
echo $sass->compile('@debug a(true); @debug a(false);');

?>
--EXPECT--
stdin:1 DEBUG: rgba(10, 20, 30, 0.4)
stdin:1 DEBUG: #0a141e
