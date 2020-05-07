--TEST--
produce error from callback
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
$sass->setFunctions([
    'a()' => function(){
        $retval = sass_make_error('error');
        assert(get_resource_type($retval) === 'Sass_Value');
        return $retval;
    },
]);
try {
    echo $sass->compile('@debug a();');
}
catch (SassException $e)
{
    echo trim($e->getMessage()) . "\n";
}

?>
--EXPECT--
Error: error in C function a: error
        on line 1:8 of stdin, in function `a`
        from line 1:8 of stdin
>> @debug a();
   -------^
