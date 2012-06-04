===================
vmod_parsepost
===================

-------------------------------
Varnish parse post data module
-------------------------------

:Author: Syohei Tanaka(@xcir)
:Date: 2012-06-03
:Version: 0.2
:Manual section: 3

SYNOPSIS
===========

import parsepost;

DESCRIPTION
==============


FUNCTIONS
============

parse
-------------

Prototype
        ::

                parse(STRING targethead, BOOL setParam, STRING paramPrefix, BOOL parseMulti ,BOOL parseFile)
Parameter
        ::

                STRING targethead
                  store raw data to specified header.
                  if this param is null. is not set.
                  
                BOOL setParam
                  true  = header is set to the data was parsed.
                  false = is not set.
                  
                STRING paramPrefix
                  prefix to use when header setting.
                  
                BOOL parseMulti
                  true  = parse to the multipart/form-data
                  false = no parse.
                  
                BOOL parseFile
                  true  = parse to the file(multipart/form-data)
                  false = no parse.
	
Return value
	INT  success = 1 ,failed < 1
Description
	get POST request(Only "application/x-www-form-urlencoded" "multipart/form-data")
Example
        ::

                if(parsepost.parse("x-raw",true,"p_",true,true) == 1){
                  std.log("raw: " + req.http.x-raw);
                  std.log("submitter: " + req.http.p_submitter);
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


Tested Version
===========

* 3.0.1
* 3.0.2
* 3.0.2-streaming
* 3.0.3-rc1

HISTORY
===========

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
