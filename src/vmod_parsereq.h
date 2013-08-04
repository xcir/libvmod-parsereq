#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

#include "vcl.h"
#include "vrt.h"
#include "bin/varnishd/cache.h"
#include "config.h"
#include "vcc_if.h"
#include "vmod_abi.h"


#define POST_REQ_HDR "\024X-VMOD-PARSEREQ-PTR:"
#define POST_REQ_HDR_NAME "X-VMOD-PARSEREQ-PTR"



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
static unsigned           hook_done          = 0;
static vcl_func_f         *vmod_Hook_miss    = NULL;
static vcl_func_f         *vmod_Hook_pass    = NULL;
static vcl_func_f         *vmod_Hook_pipe    = NULL;
static vcl_func_f         *vmod_Hook_deliver = NULL;
static vcl_func_f         *vmod_Hook_error   = NULL;

static pthread_mutex_t    vmod_mutex = PTHREAD_MUTEX_INITIALIZER;

//////////////////////////////////////////
//Debug
static unsigned           is_debug           = 0;

//////////////////////////////////////////
//for internal head
enum VMODREQ_TYPE { POST, GET, COOKIE, REQ, AUTO, NONE};

struct hdr {
	char *key;
	char *value;
	int  size;
	unsigned bin;
	unsigned array;
	VTAILQ_ENTRY(hdr) list;
};

struct vmod_headers {
	unsigned			magic;
#define VMOD_HEADERS_MAGIC 0x8d4d29ac
//	unsigned value_enabled;
	char *seek;
//	int count;//まだ値入れてないのであとで入れる
	VTAILQ_HEAD(, hdr) headers;
};



struct vmod_request {
	unsigned			magic;
#define VMOD_REQUEST_MAGIC 0x8d4f21af
	struct vmod_headers* post;
	struct vmod_headers* get;
	struct vmod_headers* cookie;
	
	struct vmod_headers* hdr_req;

	unsigned init_req;
	char seek_tmp[256];
	
	int  parse_ret;
	
	char *raw_post;
	int  size_post;

	char *raw_get;
	int  size_get;
	
	char *raw_cookie;
	int  size_cookie;

	unsigned  opt_post_lookup;
	
	enum VMODREQ_TYPE nowtype;
};



////////////////////////////////////////////////////
//user for parse
enum VMODREQ_PARSE{URL,MULTI,UNKNOWN};



ssize_t vmod_HTC_Read(struct worker *, struct http_conn *, void *, size_t );

static int vmod_Hook_unset_deliver(struct sess *);
static int vmod_Hook_unset_bereq(struct sess *);
static int vmod_Hook_unset_error(struct sess *);
static void vmod_Hook_Miss_opt_post_loopup(struct sess *);

static void vmodreq_headers_free(struct vmod_headers *);


const char *vmodreq_header(struct sess *, enum VMODREQ_TYPE , const char *);
void vmodreq_sethead(struct sess *,struct vmod_request *, enum VMODREQ_TYPE ,const char *, const char *,int);
struct vmod_headers *vmodreq_getheaders(struct sess *,struct vmod_request *, enum VMODREQ_TYPE );
struct vmod_request *vmodreq_get(struct sess *);
struct vmod_request *vmodreq_init(struct sess *);
void vmodreq_init_cookie(struct sess *,struct vmod_request *);
void vmodreq_init_get(struct sess *,struct vmod_request *);
void vmodreq_init_post(struct sess *,struct vmod_request *);
struct vmod_request *vmodreq_get_raw(struct sess *);
static void vmodreq_free(struct vmod_request *);

int decodeForm_multipart(struct sess *,char *);
int vmodreq_get_parse(struct sess *);
int vmodreq_cookie_parse(struct sess *);
int vmodreq_reqbody(struct sess *, char**,int*);
int vmodreq_post_parse(struct sess *);
void init_header(struct sess *, enum gethdr_e);

const char *vmodreq_getheader(struct sess *,struct vmod_request *, enum VMODREQ_TYPE , const char *);
int vmodreq_getheadersize(struct sess *,struct vmod_request *, enum VMODREQ_TYPE , const char *);
struct hdr *vmodreq_getrawheader(struct sess *,struct vmod_request *, enum VMODREQ_TYPE , const char *);
int vmodreq_decode_urlencode(struct sess *,char *,enum VMODREQ_TYPE,char,char,int);

int vmodreq_hdr_count(struct sess *, enum VMODREQ_TYPE );
const char *vmodreq_seek(struct sess *, enum VMODREQ_TYPE );
void vmodreq_seek_reset(struct sess *, enum VMODREQ_TYPE );

void debugmsg(struct sess *,const char*,...);

typedef int (*vcl_userdef_func)(struct sess *sp);

const char* vmod_read_cur(struct sess *, enum VMODREQ_TYPE);
unsigned vmod_read_iterate(struct sess *, const char* , enum VMODREQ_TYPE type);

int vmodreq_headersize(struct sess *, enum VMODREQ_TYPE , const char *);
enum VMODREQ_TYPE vmod_convtype(const char*);
enum gethdr_e vmod_convhdrtype(struct sess *,const char*, unsigned*);
void gen_hdrtxt(const char *, char *, int);
int count_header(struct sess *, enum gethdr_e );
struct http * vrt_selecthttp(struct sess *, enum gethdr_e);
const char*get_header_key(struct sess *, enum gethdr_e , int );
void header_iterate(struct sess *, const char* , enum gethdr_e );
void setdebug();
void chkinit(struct sess *);
