<?php

$l = <<< EOT
query	a	b	c	d	none	amulti
						
a=b	"b"					
a=b&b=cdef&c=d&d=あ	"b"	"cdef"	"d"	"あ"		
&						
a&b=c&c&d	""	"c"	""	""		
&&						
=&						
==						
&=&=&=						
&=&=&=a					"a"	
&=&=&a=b	"b"					
a=a=a	"a=a"					
a=c&a=b	"c,b"					
a[]=a&a[]=a&a[]=c						"a,a,c"
a=a&a=b&b=c	"a,b"	"c"				
EOT;

$base = <<< EOT
varnishtest "POST-urlencode"

server s1 {
       rxreq
       txresp
} -start

varnish v1 -vcl+backend {
	import parsereq from "\${vmod_topbuild}/src/.libs/libvmod_parsereq.so";

	sub vcl_recv {
		parsereq.init();
	}
	sub vcl_deliver{
		set resp.http.a        = parsereq.post_header("a");
		set resp.http.b        = parsereq.post_header("b");
		set resp.http.c        = parsereq.post_header("c");
		set resp.http.d        = parsereq.post_header("d");
		set resp.http.none     = parsereq.post_header("");
		set resp.http.amulti   = parsereq.post_header("a[]");
		set resp.http.raw      = parsereq.post_body();
	}
} -start

client c1 {
	txreq -req POST -url "/" -hdr "Content-Type: application/x-www-form-urlencoded%suf%" -body "%query%"
	rxresp
%pat%
	%query2%
}

client c1 -run

EOT;

$sl =explode("\n",$l);
//	expect resp.http.a  == "b"

$nsl=array();
foreach($sl as $v){
	$nsl[]=explode("\t",$v);
}

$cnt=count($nsl);
$key =&$nsl[0];
$tc=array();
for($i = 1 ;$i<$cnt;$i++){
	$suf="";// charset=UTF-8
	if( $i % 2 == 0)
		$suf="; charset=UTF-8";
	$val =&$nsl[$i];
	$nc = count($val);
	
	$ret="";
	
	for($x = 1 ;$x<$nc;$x++){
		if($val[$x]=="")continue;
		$ret .= "\texpect resp.http.{$key[$x]}  == {$val[$x]}\n";
		
	}
	
	if(isset($q2[1])){
		$q="expect resp.http.raw  == \"{$val[0]}\"";
	}else{
		$q='';
	}
	$q="expect resp.http.raw  == \"{$val[0]}\"";$q="expect resp.http.raw  == \"{$val[0]}\"";

	$tc[]=str_replace('%suf%',$suf,str_replace('%query2%',$q,str_replace("%pat%",$ret,str_replace("%query%",$val[0],$base))));
}
$i=0;
foreach($tc as $v){
	$i++;
	$fn = sprintf('test_POST-urlencode_%05d.vtc', $i);
	file_put_contents($fn,$v);
}

