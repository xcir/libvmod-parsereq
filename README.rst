===================
vmod_parsepost
===================

-------------------------------
Varnish parse post data module
-------------------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2012-06-10
:Version: 0.3
:Manual section: 3

SYNOPSIS
===========

import parsepost;

DESCRIPTION
==============


FUNCTIONS
============

init
-------------

Prototype
        ::

                init()

Parameter

Return value
	void

Description
	initialize.
	
	please write parsepost.init(); to 1st line in vcl_recv.

Example
        ::

                import parsepost;
                
                vcl_recv{
                  parsepost.init();
                }

XXX_header (XXX=post,get,cookie)
-----------------------------------

Prototype
        ::

                post_header(STRING key)
                get_header(STRING key)
                cookie_header(STRING key)
Parameter
        ::

                STRING key
                  data you want to get.

	
Return value
	STRING
Description
	get value.

Example
        ::

                //vcl
                vcl_deliver{
                  set resp.http.hoge = parsepost.post_header("hoge");
                }
                
                //return
                hoge: hogevalue

XXX_body (XXX=post,get,cookie)
-----------------------------------

Prototype
        ::

                post_body()
                get_body()
                cookie_body()

Parameter

Return value
	STRING

Description
	get (get,post,cookie) raw data.
	
	this function is dangerous.
	raw data is not escape.
	if you want to use ,require a thorough understanding of risk.

Example
        ::

                //vcl
                vcl_deliver{
                  set resp.http.hoge = parsepost.post_body();
                }
                
                //return
                hoge: hoge=hogevalue&mage=magevalue

INSTALLATION
==================

Installation requires Varnish source tree.

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

you try to increase the sess_workspace,http_req_size and stack size(ulimit -s)

Tested Version
===============

* 3.0.1
* 3.0.2
* 3.0.2-streaming
* 3.0.3-rc1

HISTORY
===========

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
