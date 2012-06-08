#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "vcl.h"
#include "vrt.h"
#include "bin/varnishd/cache.h"
#include "config.h"
#include "vcc_if.h"
#include "vmod_abi.h"


#define POST_REQ_HDR "\025X-VMOD-PARSEPOST-PTR:"



//#define DEBUG_HTCREAD
//#define DEBUG_SYSLOG

//////////////////////////////////////////
//Compatible use for HTC_Read
static int type_htcread = 0;
//HTC_Read ~3.0.2
typedef ssize_t HTC_READ302(struct http_conn *htc, void *d, size_t len);

//HTC_Read 3.0.3~
typedef ssize_t HTC_READ303(struct worker *w, struct http_conn *htc, void *d, size_t len);

//////////////////////////////////////////
//Hook
static vcl_func_f         *vmod_Hook_miss    = NULL;
static vcl_func_f         *vmod_Hook_pass    = NULL;
static vcl_func_f         *vmod_Hook_pipe    = NULL;
static vcl_func_f         *vmod_Hook_deliver = NULL;

static pthread_mutex_t    vmod_mutex = PTHREAD_MUTEX_INITIALIZER;

static int vmod_Hook_unset_bereq(struct sess*);
static int vmod_Hook_unset_deliver(struct sess*);

//////////////////////////////////////////
//for internal head
enum VMODREQ_TYPE { POST, GET, COOKIE };

struct hdr {
	char *key;
	char *value;
	int  size;
	unsigned bin;
	VTAILQ_ENTRY(hdr) list;
};

struct vmod_headers {
	unsigned			magic;
#define VMOD_HEADERS_MAGIC 0x8d4d29ac
	VTAILQ_HEAD(, hdr) headers;
};

struct vmod_request {
	unsigned			magic;
#define VMOD_REQUEST_MAGIC 0x8d4f21af
	struct vmod_headers* post;
	struct vmod_headers* get;
	struct vmod_headers* cookie;
	
	char *raw_post;
	char *raw_get;
	char *raw_cookie;
};

struct vmod_request *vmodreq_get_raw(struct sess*);
int vmodreq_get_parse(struct sess*);
int vmodreq_cookie_parse(struct sess*);


////////////////////////////////////////////////////
//user for parse
enum VMODREQ_PARSE{URL,MULTI,UNKNOWN};
int vmodreq_post_parse(struct sess *);

