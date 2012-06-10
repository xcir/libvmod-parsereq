<?php

$l = <<< EOT
query	a	b	c	d	none	amulti
/?						
/?a=b	"b"					
/?a=b&b=cdef&c=d&d=あ	"b"	"cdef"	"d"	"あ"		
/&						
/?a&b=c&c&d	""	"c"	""	""		
/?&						
/?&&						
/?=&						
/?==						
/?&=&=&=						
/?&=&=&=a					"a"	
/?&=&=&a=b	"b"					
/?a=a=a	"a=a"					
/?a=c&a=b	"c,b"					
/?a[]=a&a[]=a&a[]=c						"a,a,c"
/?a=a&a=b&b=c	"a,b"	"c"				
EOT;

$base = <<< EOT
varnishtest "GET"

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
		set resp.http.a        = parsereq.get_header("a");
		set resp.http.b        = parsereq.get_header("b");
		set resp.http.c        = parsereq.get_header("c");
		set resp.http.d        = parsereq.get_header("d");
		set resp.http.none     = parsereq.get_header("");
		set resp.http.amulti   = parsereq.get_header("a[]");
		set resp.http.raw      = parsereq.get_body();
	}
} -start

client c1 {
	txreq -req GET -url "%query%"
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
	$val =&$nsl[$i];
	$nc = count($val);
	
	$ret="";
	
	for($x = 1 ;$x<$nc;$x++){
		if($val[$x]=="")continue;
		$ret .= "\texpect resp.http.{$key[$x]}  == {$val[$x]}\n";
		
	}
	$q2 = explode('?',$val[0],2);
	if(isset($q2[1])){
		$q="expect resp.http.raw  == \"{$q2[1]}\"";
	}else{
		$q='';
	}

	$tc[]=str_replace('%query2%',$q,str_replace("%pat%",$ret,str_replace("%query%",$val[0],$base)));
}
$i=0;
foreach($tc as $v){
	$i++;
	$fn = sprintf('test_GET_%05d.vtc', $i);
	file_put_contents($fn,$v);
}

