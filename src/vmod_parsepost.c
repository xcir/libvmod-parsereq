#include <stdlib.h>
#include "vcl.h"
#include "vrt.h"
#include "bin/varnishd/cache.h"
#include <time.h>

#include <arpa/inet.h>
#include <syslog.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <config.h>
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

//////////////////////////////////////////
//VMOD
int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

//////////////////////////////////////////
//use for build head

static void vmodreq_free(struct vmod_request *c) {
	struct hdr *h, *h2;
	CHECK_OBJ_NOTNULL(c, VMOD_REQUEST_MAGIC);

	VTAILQ_FOREACH_SAFE(h, &c->post->headers, list, h2) {
		VTAILQ_REMOVE(&c->post->headers, h, list);
		free(h->key);
		free(h->value);
		free(h);
	}
	VTAILQ_FOREACH_SAFE(h, &c->cookie->headers, list, h2) {
		VTAILQ_REMOVE(&c->cookie->headers, h, list);
		free(h->key);
		free(h->value);
		free(h);
	}
	VTAILQ_FOREACH_SAFE(h, &c->get->headers, list, h2) {
		VTAILQ_REMOVE(&c->get->headers, h, list);
		free(h->key);
		free(h->value);
		free(h);
	}
	free(c->raw_post);
	free(c->raw_get);
	free(c->raw_cookie);
	
	FREE_OBJ(c->post);
	FREE_OBJ(c->get);
	FREE_OBJ(c->cookie);
	FREE_OBJ(c);
}

////////////////////////////////////////////////////
//user for parse
enum VMODREQ_PARSE{URL,MULTI,UNKNOWN};
int vmodreq_post_parse(struct sess *);




////////////////////////////////////////////////////
//store raw data
struct vmod_request *vmodreq_get_raw(struct sess *sp){
	const char *tmp;
	struct vmod_request *c;

	tmp = VRT_GetHdr(sp, HDR_REQ, POST_REQ_HDR);
	
	if(tmp){
		c = (struct vmod_request *)atol(tmp);
		return c;
	}
	return NULL;
}

void vmodreq_init_post(struct sess *sp,struct vmod_request *c){
	if(sp->htc->pipeline.b == NULL) return;
	int len = Tlen(sp->htc->pipeline);
	c->raw_post = calloc(1, len +1);
	c->raw_post[len]=0;
	memcpy(c->raw_post,sp->htc->pipeline.b,len);

}

void vmodreq_init_get(struct sess *sp,struct vmod_request *c){
	
	const char *url = sp->http->hd[HTTP_HDR_URL].b;
	char *sc_q;
	if(!(sc_q = strchr(url,'?'))) return;
	sc_q++;
	int len = strlen(sc_q);
	c->raw_get = calloc(1, len +1);

	c->raw_get[len]=0;
	memcpy(c->raw_get,sc_q,len);
	//normalization
	if(len>1 && c->raw_get[len-1] =='&')
		c->raw_get[len-1] = 0;
//	syslog(6,"hohoho %s",c->raw_get);
}

void vmodreq_init_cookie(struct sess *sp,struct vmod_request *c){
	const char *r = VRT_GetHdr(sp, HDR_REQ, "\007cookie:");
	if(!r) return;
	int len = strlen(r);
	c->raw_cookie = calloc(1, len +1);
	c->raw_cookie[len]=0;
	memcpy(c->raw_cookie,r,len);
}


////////////////////////////////////////////////////
//init structure
struct vmod_request *vmodreq_init(struct sess *sp){

	struct vmod_request *c;
	int r;
	char buf[64];
	buf[0] = 0;
	
	//allocate memory
	ALLOC_OBJ(c, VMOD_REQUEST_MAGIC);
	AN(c);
	ALLOC_OBJ(c->post,VMOD_HEADERS_MAGIC);
	AN(c->post);
	ALLOC_OBJ(c->get,VMOD_HEADERS_MAGIC);
	AN(c->get);
	ALLOC_OBJ(c->cookie,VMOD_HEADERS_MAGIC);
	AN(c->cookie);
//	syslog(6,"PNT   %lu %lu %lu",c->post,c->get,c->cookie);
	//assign pointer
	snprintf(buf,64,"%lu",c);
	VRT_SetHdr(sp, HDR_REQ, POST_REQ_HDR, buf,vrt_magic_string_end);

	//parse post data
	r =vmodreq_post_parse(sp);
	
	//init rawdata
	vmodreq_init_post(sp,c);
	vmodreq_init_get(sp,c);
	vmodreq_init_cookie(sp,c);

	//parse get data
	r = vmodreq_get_parse(sp);
	r = vmodreq_cookie_parse(sp);
	
	//hook for vcl function
	if(vmod_Hook_pipe == NULL || (vmod_Hook_unset_bereq == sp->vcl->pipe_func)){
		AZ(pthread_mutex_lock(&vmod_mutex));
		if(vmod_Hook_pipe == NULL){
			
			vmod_Hook_deliver		= sp->vcl->deliver_func;
			sp->vcl->deliver_func	= vmod_Hook_unset_deliver;

			vmod_Hook_miss			= sp->vcl->miss_func;
			sp->vcl->miss_func		= vmod_Hook_unset_bereq;
			
			vmod_Hook_pass			= sp->vcl->pass_func;
			sp->vcl->pass_func		= vmod_Hook_unset_bereq;
			
			vmod_Hook_pipe			= sp->vcl->pipe_func;
			sp->vcl->pipe_func		= vmod_Hook_unset_bereq;
			

		}
		AZ(pthread_mutex_unlock(&vmod_mutex));
	}
	return c;
}

////////////////////////////////////////////////////
//get structure pointer
struct vmod_request *vmodreq_get(struct sess *sp){
	struct vmod_request *c;
	c = vmodreq_get_raw(sp);
	if(c)
		return c;
	return vmodreq_init(sp);
}

////////////////////////////////////////////////////
//get headers
struct vmod_headers *vmodreq_getheaders(struct vmod_request *c, enum VMODREQ_TYPE type){
	struct vmod_headers *r = NULL;
	switch(type){
		case POST:
			r = c->post;
			break;
		case GET:
			r = c->get;
			break;
		case COOKIE:
			r = c->cookie;
			break;
	}
	return r;
}

////////////////////////////////////////////////////
//store value
void vmodreq_sethead(struct vmod_request *c, enum VMODREQ_TYPE type,const char *key, const char *value)
{
	
	struct hdr *h;
	struct vmod_headers *hs;
	hs = vmodreq_getheaders(c,type);

	h = calloc(1, sizeof(struct hdr));
	AN(h);
	
	h->key = strndup(key,strlen(key));
	AN(h->key);
	
	h->value = strndup(value,strlen(value));
	AN(h->value);
	
	
	VTAILQ_INSERT_HEAD(&hs->headers, h, list);
}

////////////////////////////////////////////////////
//get header value
const char *vmodreq_header(struct sess *sp, enum VMODREQ_TYPE type, const char *header)
{
	struct hdr *h;
	const char *r = NULL;
	struct vmod_request *c;

	c = vmodreq_get(sp);
	struct vmod_headers *hs;
	hs = vmodreq_getheaders(c,type);
	VTAILQ_FOREACH(h, &hs->headers, list) {
		if (strcasecmp(h->key, header) == 0) {
			r = h->value;
			break;
		}
	}
	return r;
}

//////////////////////////////////////////
//hook function(miss,pass,pipe)
static int vmod_Hook_unset_bereq(struct sess *sp){
//	syslog(6,"unset %s",POST_REQ_HDR);
	VRT_SetHdr(sp, HDR_BEREQ, POST_REQ_HDR, 0);
	
//	syslog(6,"unsetsd %s",VRT_GetHdr(sp, HDR_BEREQ, POST_REQ_HDR));
	switch(sp->step){
		case STP_MISS:
			return(vmod_Hook_miss(sp));

		case STP_PASS:
			return(vmod_Hook_pass(sp));

		case STP_PIPE:
			return(vmod_Hook_pipe(sp));
	}

	

}
//////////////////////////////////////////
//hook function(deliver)
static int vmod_Hook_unset_deliver(struct sess *sp){
	int ret = vmod_Hook_deliver(sp);

	struct vmod_request *c;
	c = vmodreq_get_raw(sp);
	if(c)
		vmodreq_free(c);

	return(ret);

}


const char *vmod_post_header(struct sess *sp, const char *header)
{
	return vmodreq_header(sp,POST,header);
}
const char *vmod_get_header(struct sess *sp, const char *header)
{
	return vmodreq_header(sp,GET,header);
}

const char *vmod_cookie_header(struct sess *sp, const char *header)
{
	return vmodreq_header(sp,COOKIE,header);
}
//////////////////////////////////////////
//HTC_Read
ssize_t vmod_HTC_Read(struct worker *w, struct http_conn *htc, void *d, size_t len){
	if(type_htcread == 0){
		if(
			strstr(VMOD_ABI_Version,"Varnish 3.0.2 ") ||
			strstr(VMOD_ABI_Version,"Varnish 3.0.2-") ||
			strstr(VMOD_ABI_Version,"Varnish 3.0.1 ") ||
			strstr(VMOD_ABI_Version,"Varnish 3.0.1-")
		){
			type_htcread = 1;
		}else{
			type_htcread = 2;
		}
#ifdef DEBUG_SYSLOG
		syslog(6,"HTC_Read,init:%d",type_htcread);
#endif
	}
#ifdef DEBUG_SYSLOG
	syslog(6,"HTC_Read,exec:%d",type_htcread);
#endif
	void * hr = (void *)HTC_Read;
	switch(type_htcread){
		case 1:
			return ((HTC_READ302 *)hr)(htc,d,len);
			break;
		case 2:
			return ((HTC_READ303 *)hr)(w,htc,d,len);
			break;
	}
	
		
}


unsigned __url_encode(char* url,char *copy){
	/*
	  base:http://d.hatena.ne.jp/hibinotatsuya/20091128/1259404695
	*/
	int i;
	char *pt = url;
	unsigned char c;
	char *url_en = copy;

	for(i = 0; i < strlen(pt); ++i){
		c = *url;

		if((c >= '0' && c <= '9')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z')
		|| (c == '\'')
		|| (c == '*')
		|| (c == ')')
		|| (c == '(')
		|| (c == '-')
		|| (c == '.')
		|| (c == '_')){
			*url_en = c;
			++url_en;
		}else if(c == ' '){
			*url_en = '+';
			++url_en;
		}else{
			*url_en = '%';
			++url_en;
			sprintf(url_en, "%02x", c);
			url_en = url_en + 2;
		}
		++url;
	}

	*url_en = 0;
	return(1);
}
unsigned url_encode_setHdr(struct sess *sp, char* url,char *head){
	char *copy;
	char buf[3075];
	int size = 3 * strlen(url) + 3;
	if(size > 3075){
		///////////////////////////////////////////////
		//use ws
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < size){
#ifdef DEBUG_SYSLOG
		syslog(6,"parse:url_encode_setHdr more ws size");
#endif

			return 0;
		}
		copy = (char*)sp->wrk->ws->f;
		///////////////////////////////////////////////
	}else{
		copy = buf;
	}
	__url_encode(url,copy);

//	*encoded_size += strlen(copy) + head[0] +1;
	if(size > 3075){
		WS_Release(sp->wrk->ws,strlen(copy)+1);
	}
	struct vmod_request *c = vmodreq_get(sp);	
	
	
//	head[strlen(head+1)]=0;
//	syslog(6,"store-NX->head [%s] [%s]",head,copy);
	vmodreq_sethead(c,POST,head,copy);


	return(1);
}

int decodeForm_multipart(struct sess *sp,char *body){
	

	char tmp,tmp2;
	char sk_boundary[258];
	char *raw_boundary, *h_ctype_ptr;
	char *p_start, *p_end, *p_body_end, *name_line_end, *name_line_end_tmp, *start_body, *sc_name, *sc_filename;
	int  boundary_size ,idx;
	
	char *tmpbody    = body;
	char *basehead   = 0;
	char *curhead;
	int  ll_u, ll_head_len, ll_size;
	int  ll_entry_count = 0;

	int u;
	char *orig_cv_body, *cv_body, *h_val;
	
	//////////////////////////////
	//get boundary
#ifdef DEBUG_SYSLOG
		syslog(6,"GetHdr Content-Type:");
#endif

	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	raw_boundary = strstr(h_ctype_ptr,"; boundary=");
	if(!raw_boundary || strlen(raw_boundary) > 255){
#ifdef DEBUG_SYSLOG
		syslog(6,"parse:decodeForm_multipart boudary size>255");
#endif
		return -5;
	}
	
	raw_boundary   +=11;
	sk_boundary[0] = '-';
	sk_boundary[1] = '-';
	sk_boundary[2] = 0;
	strncat(sk_boundary,raw_boundary,strlen(raw_boundary));
	boundary_size = strlen(sk_boundary);


	p_start = strstr(tmpbody,sk_boundary);
	while(1){
		///////////////////////////////////////////////
		//search data
		if(!p_start) break;

		//boundary
		p_end = strstr(p_start + 1,sk_boundary);
		if(!p_end) break;
		//name
		sc_name = strstr(p_start +boundary_size,"name=\"");
		if(!sc_name) break;
		sc_name+=6;
		//line end
		name_line_end  = strstr(sc_name,"\"\r\n");
		name_line_end_tmp = strstr(sc_name,"\"; ");
		if(!name_line_end) break;
		
		if(name_line_end_tmp && name_line_end_tmp < name_line_end)
			name_line_end = name_line_end_tmp;
		
		//body
		start_body = strstr(sc_name,"\r\n\r\n");
		if(!start_body) break;
		//filename
		sc_filename  = strstr(sc_name,"; filename");

		///////////////////////////////////////////////
		//build head
		if((name_line_end -1)[0]=='\\'){
			idx = 1;
		}else{
			idx = 0;
		}
		tmp                   = name_line_end[idx];
		name_line_end[idx]    = 0;
		

		///////////////////////////////////////////////
		//filecheck
//		if(!parseFile){
//			if(sc_filename && sc_filename < start_body){
//				//content is file(skip)
//				p_start = p_end;
//				continue;
//			}
//		}
		
		
		///////////////////////////////////////////////
		//url encode & set header
		p_body_end    = p_end -2;
		start_body    +=4;
		tmp2          = p_body_end[0];
		p_body_end[0] = 0;
		//bodyをURLエンコードする

		if(!url_encode_setHdr(sp,start_body,sc_name)){
			//メモリない
			name_line_end[idx]	= tmp;
			p_body_end[0]		= tmp2;
			return -1;
		}
		
		name_line_end[idx]		= tmp;
		p_body_end[0]			= tmp2;

		///////////////////////////////////////////////
		//check last boundary
		if(!p_end) break;
		
		p_start = p_end;
	}

	return 1;
}
const char* vmod_post_body(struct sess *sp){
	struct vmod_request *c = vmodreq_get(sp);
	return c->raw_post;
}
const char* vmod_get_body(struct sess *sp){
	struct vmod_request *c = vmodreq_get(sp);
	return c->raw_get;
}
const char* vmod_cookie_body(struct sess *sp){
	struct vmod_request *c = vmodreq_get(sp);
	return c->raw_cookie;
}


int decodeForm_urlencoded(struct sess *sp,char *body,enum VMODREQ_TYPE type){
	char *sc_eq,*sc_amp;
	char *tmpbody = body;
	char* tmphead;
	char tmp,tmp2;
	struct vmod_request *c = vmodreq_get(sp);

	
	while(1){
		//////////////////////////////
		//search word
		if(!(sc_eq = strchr(tmpbody,'='))){
			break;
		}
		if(!(sc_amp = strchr(tmpbody,'&'))){
			sc_amp = sc_eq + strlen(tmpbody);
		}
		
		//////////////////////////////
		//build head
		tmp=sc_eq[0];
		sc_eq[0] = 0;// = -> null
		

		tmphead = tmpbody;
		

		//////////////////////////////
		//build body
		tmpbody   = sc_eq + 1;
		tmp2=sc_amp[0];
		sc_amp[0] = 0;// & -> null

		//////////////////////////////
		//set header
		
//		syslog(6,"store-->head %s %s",tmphead,tmpbody);
		vmodreq_sethead(c,type,tmphead,tmpbody);
		
		
		sc_eq[0]  = tmp;
		sc_amp[0] = tmp2;
		tmpbody   = sc_amp + 1;
	}
	return 1;
}
int vmodreq_get_parse(struct sess *sp){
	int ret=1;
	struct vmod_request *c = vmodreq_get(sp);
	if(c->raw_get)
		ret = decodeForm_urlencoded(sp, c->raw_get,GET);
	return ret;
}

int vmodreq_cookie_parse(struct sess *sp){
	int ret=1;
	struct vmod_request *c = vmodreq_get(sp);
	if(!c->raw_cookie) return ret;
	
	char *sc_eq,*sc_amp;
	char *tmpbody = c->raw_cookie;
	char* tmphead;
	char tmp,tmp2;
	
	

	
	while(1){
		//////////////////////////////
		//search word
		if(!(sc_eq = strchr(tmpbody,'='))){
			break;
		}
		if(!(sc_amp = strchr(tmpbody,';'))){
			sc_amp = sc_eq + strlen(tmpbody);
		}
		
		//////////////////////////////
		//build head
		tmp=sc_eq[0];
		sc_eq[0] = 0;// = -> null
		

		tmphead = tmpbody;
		

		//////////////////////////////
		//build body
		tmpbody   = sc_eq + 1;
		tmp2= sc_amp[0];
		sc_amp[0] = 0;// & -> null

		//////////////////////////////
		//set header
		
//		syslog(6,"store-->head %s %s",tmphead,tmpbody);
		vmodreq_sethead(c,COOKIE,tmphead,tmpbody);
		
		
		sc_eq[0]  = tmp;
		sc_amp[0] = tmp2;
		tmpbody   = sc_amp + 1;
		while(1){
//			syslog(6,"[%s]",tmpbody);
			if(tmpbody[0]==0)break;
			if(tmpbody[0] ==' '){
				++tmpbody;
			}else{
				break;
			}
		}
	}
	return 1;
	
	
	return ret;
}

int vmodreq_post_parse(struct sess *sp){

/*
	1	=sucess
	-1	=error		less sess_workspace
	-2	=error		none content-length key
	-3	=error		less content-length
	-4	=error		unknown or none content-type
*/	
	unsigned long		content_length,orig_content_length;
	char 				*h_clen_ptr, *h_ctype_ptr, *body;
	int					buf_size, rsize;
	char				buf[1024];
	enum VMODREQ_PARSE	exec;

	//////////////////////////////
	//check Content-Type

	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	
	if(h_ctype_ptr != NULL){
		if      (h_ctype_ptr == strstr(h_ctype_ptr, "application/x-www-form-urlencoded")) {
			//application/x-www-form-urlencoded
			exec = URL;
		}else if(h_ctype_ptr == strstr(h_ctype_ptr, "multipart/form-data")){
			//multipart/form-data
			exec = MULTI;
		}else{
			exec = UNKNOWN;
		}
	}else{
		//none support type
		exec = UNKNOWN;
	}
	
	//thinking....
	if(exec == UNKNOWN) return -4;
	
	
	//////////////////////////////
	//check Content-Length
	h_clen_ptr = VRT_GetHdr(sp, HDR_REQ, "\017Content-Length:");
	if (!h_clen_ptr) {
		//can't get
		return -2;
	}
	orig_content_length = content_length = strtoul(h_clen_ptr, NULL, 10);
	if (content_length <= 0) {
		//illegal length
		return -2;
	}
	//////////////////////////////
	//Check POST data is loaded
	if(sp->htc->pipeline.b != NULL && Tlen(sp->htc->pipeline) == content_length){
		//complete read
		body = sp->htc->pipeline.b;
	}else{
		//incomplete read
		int rxbuf_size = Tlen(sp->htc->rxbuf);
		///////////////////////////////////////////////
		//use ws
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < content_length + rxbuf_size + 1){
			return -1;
		}
		body = (char*)sp->wrk->ws->f;
		memcpy(body, sp->htc->rxbuf.b, rxbuf_size);
		sp->htc->rxbuf.b = body;
		body += rxbuf_size;
		body[0]= 0;
		sp->htc->rxbuf.e = body;
		WS_Release(sp->wrk->ws,content_length + rxbuf_size + 1);
		///////////////////////////////////////////////
		
		//////////////////////////////
		//read post data
		while (content_length) {
			if (content_length > sizeof(buf)) {
				buf_size = sizeof(buf) - 1;
			}
			else {
				buf_size = content_length;
			}

			// read body data into 'buf'
			//rsize = HTC_Read(sp->htc, buf, buf_size);
			rsize = vmod_HTC_Read(sp->wrk, sp->htc, buf, buf_size);
			if (rsize <= 0) {
				return -3;
			}

			content_length -= rsize;

			strncat(body, buf, buf_size);
		}
		sp->htc->pipeline.b = body;
		sp->htc->pipeline.e = body + orig_content_length;
	}

	//decode form
	int ret = 1;
	switch(exec){
		case URL:
			ret = decodeForm_urlencoded(sp, body,POST);
			break;
		case MULTI:
			ret = decodeForm_multipart(sp, body);
			break;
		case UNKNOWN:
			break;
		
	}
	return ret;

}
