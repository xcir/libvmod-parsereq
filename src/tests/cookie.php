<?php

$l = <<< EOT
query	a	b	c	d	none	amulti
a=b;	"b"					
a=b; b=cdef; c=あ	"b"	"cdef"	"あ"			
a=b=c=d; b=c	"b=c=d"	"c"				
;;a=b;;b=c;;;	"b"	"c"				
a=a; a=b	"a,b"					
a==b;	"=b"					
a=b	"b"					
;=;=;=						
;=;=;==					"="	
a[]=a; b=c;a[]=b		"c"				"a,b"
EOT;

$base = <<< EOT
varnishtest "COOKIE"

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
		set resp.http.a        = parsereq.cookie_header("a");
		set resp.http.b        = parsereq.cookie_header("b");
		set resp.http.c        = parsereq.cookie_header("c");
		set resp.http.d        = parsereq.cookie_header("d");
		set resp.http.none     = parsereq.cookie_header("");
		set resp.http.amulti   = parsereq.cookie_header("a[]");
		set resp.http.raw      = parsereq.cookie_body();
	}
} -start

client c1 {
	txreq -req GET -url "/" -hdr "Cookie: %query%"
	rxresp
%pat%
	expect resp.http.raw  == "%query%"

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
	$val =&$nsl[$i];
	$nc = count($val);
	
	$ret="";
	
	for($x = 1 ;$x<$nc;$x++){
		if($val[$x]=="")continue;
		$ret .= "\texpect resp.http.{$key[$x]}  == {$val[$x]}\n";
		
	}
	$tc[]=str_replace("%pat%",$ret,str_replace("%query%",$val[0],$base));
}
$i=0;
foreach($tc as $v){
	$i++;
	$fn = sprintf('test_COOKIE_%05d.vtc', $i);
	file_put_contents($fn,$v);
}

