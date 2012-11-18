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
		set resp.http.a        = parsereq.param(cookie, "a");
		set resp.http.b        = parsereq.param(cookie, "b");
		set resp.http.c        = parsereq.param(cookie, "c");
		set resp.http.d        = parsereq.param(cookie, "d");
		set resp.http.none     = parsereq.param(cookie, "");
		set resp.http.amulti   = parsereq.param(cookie, "a[]");
		set resp.http.raw      = parsereq.body(cookie);
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
	$fn = sprintf('test_02_COOKIE_%05d.vtc', $i);
	file_put_contents($fn,$v);
}

