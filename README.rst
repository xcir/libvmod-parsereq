===================
vmod_parsereq
===================

-------------------------
Varnish parse data module
-------------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2012-08-02
:Version: 0.6
:Manual section: 3

SYNOPSIS
===========

import parsereq;

DESCRIPTION
==============

ATTENTION
============

Don’t use "vcl.use" and "vcl.discard" on VCL with the vmod loaded. This will cause the a segfault. 

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
	get status code.

Example
        ::

                import parsereq;
                
                vcl_recv{
                  if(parsereq.errorcode()<1){
                  
                    ...
                  
                  }
                }

post_header/get_header/cookie_header
------------------------------------

Prototype
        ::

                post_header(STRING key)
                get_header(STRING key)
                cookie_header(STRING key)
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
                  set resp.http.hoge = parsereq.post_header("hoge");
                }
                
                //return
                hoge: hogevalue

post_body/get_body/cookie_body
------------------------------

Prototype
        ::

                post_body()
                get_body()
                cookie_body()

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
                  set resp.http.hoge = parsereq.post_body();
                }
                
                //return
                hoge: hoge=hogevalue&mage=magevalue


post_read_keylist/get_read_keylist/cookie_read_keylist
------------------------------------------------------

Prototype
        ::

                post_read_keylist()
                get_read_keylist()
                cookie_read_keylist()

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
                  set resp.http.n1 = parsereq.get_read_keylist();
                  set resp.http.n2 = parsereq.get_read_keylist();
                  //nothing
                  set resp.http.n3 = parsereq.get_read_keylist();
                }
                
                //return
                n1: name2
                n2: name1

post_seek_reset/get_seek_reset/cookie_seek_reset
------------------------------------------------

Prototype
        ::

                post_seek_reset()
                get_seek_reset()
                cookie_seek_reset()

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
                  set resp.http.n1 = parsereq.get_read_keylist();
                  set resp.http.n2 = parsereq.get_read_keylist();
                  parsereq.get_seek_reset();
                  set resp.http.n3 = parsereq.get_read_keylist();
                  set resp.http.n4 = parsereq.get_read_keylist();
                  //nothing
                  set resp.http.n5 = parsereq.get_read_keylist();
                }
                
                //return
                n1: name2
                n2: name1
                n3: name2
                n4: name1


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

* 3.0.1
* 3.0.2
* 3.0.2-streaming
* 3.0.3

HISTORY
===========

Version 0.6: Bug fix: when you vcl reloaded, hook method be off.

Version 0.5: rename module(parsepost -> parsereq)

Version 0.4: add get keylist function.

Version 0.3: support GET,COOKIE, modify interface.

Version 0.2: rename module(postparse -> parsepost)

Version 0.1: add function parse

COPYRIGHT
=============

This document is licensed under the same license as the
libvmod-rewrite project. See LICENSE for details.

* Copyright (c) 2012 Syohei Tanaka(@xcir)

File layout and configuration based on libvmod-example

* Copyright (c) 2011 Varnish Software AS

parse method based on VFW( https://github.com/scarpellini/VFW )

url encode method based on http://d.hatena.ne.jp/hibinotatsuya/20091128/1259404695
