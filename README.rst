===================
vmod_parsereq
===================

-------------------------
Varnish parse data module
-------------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2013-06-25
:Version: 0.12
:Manual section: 3

SYNOPSIS
===========

import parsereq;

DESCRIPTION
==============

SUPPORT DATA TYPES
===================

* POST
* GET
* COOKIE
* REQ(req.http.*)
* AUTO(auto type using for in subroutine only, that is called by iterate function.)

ATTENTION
============

Don't use "vcl.use" and "vcl.discard" on VCL with the vmod loaded. This will cause the a segfault. 

FUNCTIONS
============

init
-------------

Prototype
        ::

                init()

Parameter

Return value
        ::

                void
                

Description
	initialize.
	
	Make sure to call parsereq.init() in vcl_recv before using 
	any other function.

Example
        ::

                import parsereq;
                
                vcl_recv{
                  parsereq.init();
                }

setopt
-------------

Prototype
        ::

                setopt(enum {enable_post_lookup})

Option
        ::

                enable_post_lookup (EXPERIMENTAL)
                 Enable to "CACHE" for POST request. very dangerous.
                 This option modify to
                  Backend request method set to "POST".
                  Enable send body.
                  Content-Length is set to POST size.

Return value
        ::

                void
                

Description
	Set option.

Example
        ::

                import parsereq;
                
                vcl_recv{
                  parsereq.init();
                }

errcode
-------------

Prototype
        ::

                errcode()

Parameter

Return value
        ::

                int errorcode
                
                (sucess) 2   Unknown or missing content-type.
                (sucess) 1   Parse complete.
                (error)  -1  Out of session workspace. Try to increase sess_workspace
                (error)  -2  No content-length key.
                (error)  -3  Readable data < content-length.
                

Description
	Get status code.

Example
        ::

                import parsereq;
                
                vcl_recv{
                  if(parsereq.errcode()<1){
                  
                    ...
                  
                  }
                }

param/post_header/get_header/cookie_header
--------------------------------------------

Prototype
        ::

                post_header(STRING key) (LEGACY)
                get_header(STRING key) (LEGACY)
                cookie_header(STRING key) (LEGACY)
                param(enum {post, get, cookie, req, auto}, STRING key)
Parameter
        ::

                STRING key
                  Desired key value 

	
Return value
	STRING (not escaped)
Description
	Get value.

Example
        ::

                vcl_deliver{
                  set resp.http.hoge = parsereq.param(post, "hoge");
                }
                
                //return
                hoge: hogevalue

body/post_body/get_body/cookie_body
--------------------------------------

Prototype
        ::

                post_body() (LEGACY)
                get_body() (LEGACY)
                cookie_body() (LEGACY)
                body(enum {post, get, cookie})

Parameter

Return value
	STRING (NOT ESCAPED)

Description
	Get (get,post,cookie) raw data.
	
	This function is dangerous. The raw data is not escaped.
	Usage of this require a thorough understanding of the risks
	involved.

Example
        ::

                //vcl
                vcl_deliver{
                  set resp.http.hoge = parsereq.body(post);
                }
                
                //return
                hoge: hoge=hogevalue&mage=magevalue


next_key/post_read_keylist/get_read_keylist/cookie_read_keylist
-----------------------------------------------------------------

Prototype
        ::

                post_read_keylist() (LEGACY)
                get_read_keylist() (LEGACY)
                cookie_read_keylist() (LEGACY)
                next_key(enum {post, get, cookie, req, auto})

Parameter

Return value
	STRING

Description
	get (get,post,cookie) key name.

Example
        ::

                //req
                /?name1=a&name2=b
                
                //vcl
                vcl_deliver{
                  set resp.http.n1 = parsereq.next_key(get);
                  set resp.http.n2 = parsereq.next_key(get);
                  //nothing
                  set resp.http.n3 = parsereq.next_key(get);
                }
                
                //return
                n1: name2
                n2: name1

reset_offset/post_seek_reset/get_seek_reset/cookie_seek_reset
--------------------------------------------------------------

Prototype
        ::

                post_seek_reset() (LEGACY)
                get_seek_reset() (LEGACY)
                cookie_seek_reset() (LEGACY)
                reset_offset(enum {post, get, cookie, req, auto})

Parameter

Return value
	VOID

Description
	Reset the seek index.

Example
        ::

                //req
                /?name1=a&name2=b
                
                //vcl
                vcl_deliver{
                  set resp.http.n1 = parsereq.next_key(get);
                  set resp.http.n2 = parsereq.next_key(get);
                  parsereq.reset_offset(get);
                  set resp.http.n3 = parsereq.next_key(get);
                  set resp.http.n4 = parsereq.next_key(get);
                  //nothing
                  set resp.http.n5 = parsereq.next_key(get);
                }
                
                //return
                n1: name2
                n2: name1
                n3: name2
                n4: name1


size
------------------------------------------------

Prototype
        ::

                size(enum {post, get, cookie, req, auto}, STRING key)

Parameter
        ::

                STRING key
                  Desired key value 

	
Return value
	INT

Description
	Get the size of value.

Example
        ::

                //req
                /?name1=a&name2=bbb
                
                //vcl
                vcl_deliver{
                  set resp.http.n1 = parsereq.size(get, "name1");
                  set resp.http.n2 = parsereq.size(get, "name2");
                  //nothing
                  set resp.http.na = parsereq.size(get, "name99");
                }
                
                //return
                n1: 1
                n2: 3
                na: 0

current_key
-----------------------------------------------------------

Prototype
        ::

                current_key(enum {post, get, cookie, req, auto})

Parameter

	
Return value
	STRING

Description
	Get current key-name of the offset.

Example
        ::

                //req
                /?name1=a&name2=bbb
                
                //vcl
                vcl_deliver{
                  set resp.http.t1 = ">>" + parsereq.current_key(get);
                  parsereq.next_offset(get);
                  set resp.http.t2 = ">>" + parsereq.current_key(get);
                  parsereq.next_offset(get);
                  set resp.http.t3 = ">>" + parsereq.current_key(get);
                  parsereq.next_offset(get);
                  set resp.http.t4 = ">>" + parsereq.current_key(get);
                }
                
                //return
                t1: >>
                t2: >>name2
                t3: >>name1
                t4: >>name1
                


next_offset
-------------------------------------------------------------

Prototype
        ::

                next_offset(enum {post, get, cookie, req, auto})

Parameter

	
Return value
	VOID

Description
	Change to the next key.
	If next key isn't exist, will not change.


Example
        ::

                //req
                /?name1=a&name2=bbb
                
                //vcl
                vcl_deliver{
                  set resp.http.t1 = ">>" + parsereq.current_key(get);
                  parsereq.next_offset(get);
                  set resp.http.t2 = ">>" + parsereq.current_key(get);
                  parsereq.next_offset(get);
                  set resp.http.t3 = ">>" + parsereq.current_key(get);
                  parsereq.next_offset(get);
                  set resp.http.t4 = ">>" + parsereq.current_key(get);
                }
                
                //return
                t1: >>
                t2: >>name2
                t3: >>name1
                t4: >>name1

iterate(EXPERIMENTAL)
----------------------------------------------------------------

Prototype
        ::

                iterate(enum {post, get, cookie, req}, STRING)

Parameter
	STRING subroutine pointer

Return value
	BOOL

Description
	Count all elements in parameter for iterate the subroutine.
	This function is subject to change without notice.



Example
        ::

                //req
                /?name1=a&name2=bbb
                
                //vcl
                sub iterate {
                  set req.http.hoge = req.http.hoge + parsereq.current_key(auto) + ":";
                  set req.http.hoge = req.http.hoge + parsereq.param(auto, parsereq.current_key(auto)) + " ";
                }
                sub vcl_recv {
                  parsereq.init();
                  if(1 == 0){
                    call iterate;
                  }
                  set req.http.hoge= "";
                  C{
                    if(Vmod_Func_parsereq.iterate(sp, "get", (const char*)VGC_function_iterate)) return(1);
                  }C

                }
                sub vcl_deliver{
                  set resp.http.t1 = req.http.hoge;
                }
                
                //return
                t1: name2:bbb name1:a 
                



INSTALLATION
==================

Installation requires a Varnish source tree.

Usage::

 ./autogen.sh
 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod. Both the `VARNISHSRC` and `VARNISHSRC/include`
will be added to the include search paths for your module.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``

Trouble shooting
=================

You could try to increase the sess_workspace and http_req_size
parameters and stack size(ulimit -s).

Tested Version
===============

* 3.0.1(x86_64)
* 3.0.2(x86_64)
* 3.0.2-streaming(x86_64)
* 3.0.3(x86, x86_64)
* 3.0.4(x86_64)

HISTORY
===========

Version 0.12: Support POST request cache(issue #7 thanks nshahzad). 3.0.4 support(issue #8)

Version 0.11: Support REQ data type.(req.http.*) And AUTO data type.

Version 0.10: Add: param, size, body, next_key, next_offset, current_key, iterate, reset_offset

Version 0.9: Bug fix: always segfault on x86. And sometimes segfault on x86_64. [issue #5 Thanks comotion]

Version 0.8: Support unknown content-type.(post_body only) [issue #3 Thanks c0ze]

Version 0.7: Bug fix: forgot to care binary. [issue #4 Thanks dnewhall]

Version 0.6: Bug fix: when you vcl reloaded, hook method be off.

Version 0.5: Rename module(parsepost -> parsereq)

Version 0.4: Add get keylist function.

Version 0.3: Support GET,COOKIE, modify interface.

Version 0.2: Rename module(postparse -> parsepost)

Version 0.1: Add function parse

COPYRIGHT
=============

This document is licensed under the same license as the
libvmod-rewrite project. See LICENSE for details.

* Copyright (c) 2012 Syohei Tanaka(@xcir)

File layout and configuration based on libvmod-example

* Copyright (c) 2011 Varnish Software AS

parse method based on VFW( https://github.com/scarpellini/VFW )

url encode method based on http://d.hatena.ne.jp/hibinotatsuya/20091128/1259404695
