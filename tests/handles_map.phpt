--TEST--
correctly handles setting and getting source map formats
--SKIPIF--
<?php if (!extension_loaded("sass")) print "skip"; ?>
--FILE--
<?php

$sass = new Sass();
// test default from constructor
$sass->setComments(true);
$sass->setMapPath(__DIR__.'/support/test.css.map');
$css = $sass->compileFile(__DIR__.'/support/test.scss');
echo $css[1];

?>
--EXPECT--
{
	"version": 3,
	"file": "test.css",
	"sources": [
		"test.scss"
	],
	"names": [],
	"mappings": "AAAA,OAAO,CAAC,0BAAI;;AAEZ,AAAA,GAAG,CAAC;EACF,IAAI,EAAE,4BAAiD,GACxD;;;AAID,AAAA,GAAG,CAAC;EACA,IAAI,EAAE,YAAa,GACtB"
}
