===================
vmod_postparse
===================

-------------------------------
Varnish parse post data module
-------------------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2012-06-03
:Version: 0.1
:Manual section: 3

SYNOPSIS
===========

import postparse;

DESCRIPTION
==============


FUNCTIONS
============

parse
-------------

Prototype
        ::

                parse(STRING targethead, BOOL setParam, STRING paramPrefix, STRING parseMulti ,BOOL parseFile)
Return value
	INT
Description
	get POST request(Only "application/x-www-form-urlencoded" "multipart/mixed")
Example
        ::

                if(postparse.parse("x-test",false) == 1){
                  std.log("raw: " + req.http.x-test);
                  std.log("submitter: " + req.http.submitter);
                }

                //response
                12 VCL_Log      c raw: submitter=abcdef&submitter2=b
                12 VCL_Log      c submitter: abcdef


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


HISTORY
===========

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
