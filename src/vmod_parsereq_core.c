#include "vmod_parsereq.h"

/*
	todo:
		urlencode系でのsetheader系がカオスなので見直す
		vmodreq_setheadがカオスってる
		リビルドをするときはseekをNULLにリセット
*/
//////////////////////////////////////////
//VMOD

//////////////////////////////////////////
//パースした内容を開放する
static void vmodreq_headers_free(struct vmod_headers *obj){
	struct hdr *h, *h2;
	VTAILQ_FOREACH_SAFE(h, &obj->headers, list, h2) {
		VTAILQ_REMOVE(&obj->headers, h, list);
		if(h->value)
			free(h->value);
		
		free(h->key);
		free(h);
	}
	FREE_OBJ(obj);

}

static void vmodreq_free(struct vmod_request *c) {
	struct hdr *h, *h2;
	CHECK_OBJ_NOTNULL(c, VMOD_REQUEST_MAGIC);
	
	free(c->raw_post);
	free(c->raw_get);
	free(c->raw_cookie);
	
	
	vmodreq_headers_free(c->post);
	vmodreq_headers_free(c->cookie);
	vmodreq_headers_free(c->get);
	
	
	vmodreq_headers_free(c->hdr_req);


	FREE_OBJ(c);
}


////////////////////////////////////////////////////
//格納したパース構造体へのポインタを取得
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


////////////////////////////////////////////////////
//各種データ（post,get,cookie）を取得する
void vmodreq_init_post(struct sess *sp,struct vmod_request *c){
	if(sp->htc->pipeline.b == NULL) return;
	int len = Tlen(sp->htc->pipeline);
	c->raw_post = calloc(1, len +1);
	AN(c->raw_post);
	c->size_post = len;
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
	AN(c->raw_get);
	c->size_get = len;

	c->raw_get[len]=0;
	memcpy(c->raw_get,sc_q,len);
	//normalization
//	if(len>1 && c->raw_get[len-1] =='&')
//		c->raw_get[len-1] = 0;
}

void vmodreq_init_cookie(struct sess *sp,struct vmod_request *c){
	const char *r = VRT_GetHdr(sp, HDR_REQ, "\007cookie:");
	if(!r) return;
	int len = strlen(r);
	c->raw_cookie = calloc(1, len +1);
	AN(c->raw_cookie);
	c->size_cookie = len;
	c->raw_cookie[len]=0;
	memcpy(c->raw_cookie,r,len);
}


////////////////////////////////////////////////////
//構造体を初期化
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
//	c->post->value_enabled = (1==1);
	
	ALLOC_OBJ(c->get,VMOD_HEADERS_MAGIC);
	AN(c->get);
//	c->get->value_enabled = (1==1);
	
	ALLOC_OBJ(c->cookie,VMOD_HEADERS_MAGIC);
	AN(c->cookie);
//	c->cookie->value_enabled = (1==1);
	
	
	ALLOC_OBJ(c->hdr_req,VMOD_HEADERS_MAGIC);
	AN(c->hdr_req);

	//assign pointer
	snprintf(buf,64,"%ld",c);
	VRT_SetHdr(sp, HDR_REQ, POST_REQ_HDR, buf,vrt_magic_string_end);

	//parse post data
	r =vmodreq_post_parse(sp);
	c->parse_ret = r;
	
	//init rawdata
	vmodreq_init_post(sp,c);
	vmodreq_init_get(sp,c);
	vmodreq_init_cookie(sp,c);
	//parse get data
	r = vmodreq_get_parse(sp);
	r = vmodreq_cookie_parse(sp);
	
	//hook for vcl function
	if(hook_done == 1 && sp->vcl->deliver_func != vmod_Hook_unset_deliver) hook_done = 0;
	
	if(hook_done == 0){
		AZ(pthread_mutex_lock(&vmod_mutex));
		if(hook_done == 0){
			
			vmod_Hook_deliver		= sp->vcl->deliver_func;
			sp->vcl->deliver_func	= vmod_Hook_unset_deliver;

			vmod_Hook_miss			= sp->vcl->miss_func;
			sp->vcl->miss_func		= vmod_Hook_unset_bereq;
			
			vmod_Hook_pass			= sp->vcl->pass_func;
			sp->vcl->pass_func		= vmod_Hook_unset_bereq;
			
			vmod_Hook_pipe			= sp->vcl->pipe_func;
			sp->vcl->pipe_func		= vmod_Hook_unset_bereq;
			
			hook_done				= 1;

		}
		AZ(pthread_mutex_unlock(&vmod_mutex));
	}
	return c;
}



////////////////////////////////////////////////////
//構造体へのポインタを取得（初期化されてないときは初期化を行う）
//get structure pointer
struct vmod_request *vmodreq_get(struct sess *sp){
	struct vmod_request *c;
	c = vmodreq_get_raw(sp);
	if(c)
		return c;
	return vmodreq_init(sp);
}

////////////////////////////////////////////////////
//ヘッダフィールドを格納しているポインタを返却
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
		case REQ:
			r = c->hdr_req;
			break;
	}
	return r;
}

////////////////////////////////////////////////////
//値を格納
//store value
void vmodreq_sethead(struct vmod_request *c, enum VMODREQ_TYPE type,const char *key, const char *value,int size)
{
	if((!key || key[0] == 0) && size ==0) return;
	struct hdr *exhead;

	//
	struct hdr *h;
	struct vmod_headers *hs;
	
	int ndsize = 0;
	hs = vmodreq_getheaders(c,type);
	exhead = vmodreq_getrawheader(c,type,key);

	if(exhead){
		//既に値が存在する場合はカンマ区切りで連結
		h = exhead;
		//array key
		ndsize = h->size;
		char *tmp= calloc(1,size + 2 + ndsize);
		AN(tmp);
		memcpy(tmp,h->value,ndsize);
		free(h->value);
		
		/////////////
		h->value =tmp;
		h->value[ndsize]=',';
		h->array = 1;
		++ndsize;
		memcpy(h->value + ndsize,value,size);
	}else{
		h = calloc(1, sizeof(struct hdr));
		AN(h);
		h->key = strndup(key,strlen(key));
		AN(h->key);
		h->value = calloc(1,size+1);
		AN(h->value);
		
		/////////////
		memcpy(h->value,value,size);

	}
	//h->value = strndup(value,strlen(value));
	if(strlen(value) == size + ndsize){
		h->bin = 0;
	}else{
		h->bin = 1;
	}
	h->size = size + ndsize;
	if(!exhead)
		VTAILQ_INSERT_HEAD(&hs->headers, h, list);
}



////////////////////////////////////////////////////
//オフセットをリセットする
void vmodreq_seek_reset(struct sess *sp, enum VMODREQ_TYPE type)
{
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	struct vmod_headers *hs= vmodreq_getheaders(c,type);
	switch(type){
	case REQ:
		init_header(sp, HDR_REQ);
		break;
	default:
		hs->seek = NULL;
	}
}



////////////////////////////////////////////////////
//現在のオフセットのキーを取得
const char* vmod_read_cur(struct sess *sp, enum VMODREQ_TYPE type){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);

	struct vmod_headers *hs;
	hs = vmodreq_getheaders(c,type);
	return hs->seek;
}
const char* vmod_readheader_cur(struct sess *sp, enum gethdr_e where){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
/*
	struct vmod_request *c = vmodreq_get(sp);
	switch (where) {
	case HDR_REQ:
		c->seek_idx_req
		break;
	case HDR_BEREQ:
		hp = sp->wrk->bereq;
		break;
	case HDR_BERESP:
		hp = sp->wrk->beresp;
		break;
	case HDR_RESP:
		hp = sp->wrk->resp;
		break;
	case HDR_OBJ:
		CHECK_OBJ_NOTNULL(sp->obj, OBJECT_MAGIC);
		hp = sp->obj->http;
		break;
	default:
		INCOMPL();
	}
*/

}
////////////////////////////////////////////////////
//反復処理を行う
void vmod_read_iterate(struct sess *sp, const char* p, enum VMODREQ_TYPE type){
	vmodreq_seek_reset(sp,type);
	int max = vmodreq_hdr_count(sp, type);
	vcl_userdef_func func = (vcl_userdef_func)p;
	
	for(int i=0; i<max; i++){
		vmodreq_seek(sp,type);
		if(type == REQ && strcmp(vmod_read_cur(sp,type), POST_REQ_HDR_NAME) == 0)
			continue;
		func(sp);
	}
}

////////////////////////////////////////////////////
//各ヘッダの個数を取得
int vmodreq_hdr_count(struct sess *sp, enum VMODREQ_TYPE type)
{
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	struct hdr *h;

	struct vmod_headers *hs;
	hs = vmodreq_getheaders(c,type);
	char * seh = hs->seek;
	int i = 0;
	VTAILQ_FOREACH(h, &hs->headers, list) {
		++i;
	}

	return i;
}


////////////////////////////////////////////////////
//オフセットを移動する
const char *vmodreq_seek(struct sess *sp, enum VMODREQ_TYPE type)
{
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	struct hdr *h;
	struct hdr *r = NULL;
	
	switch(type){
	case REQ:
		if(!c->init_req)
			init_header(sp, HDR_REQ);
		break;
	}

	struct vmod_headers *hs;
	hs = vmodreq_getheaders(c,type);
	char * seh = hs->seek;

	
	VTAILQ_FOREACH(h, &hs->headers, list) {
		if(seh == NULL){
			r = h;
			break;
		}
		if(!h->key && strcasecmp("", seh) == 0){
			seh = NULL;
			continue;
		}else if (strcasecmp(h->key, seh) == 0) {
			seh = NULL;
			continue;
		}
	}
	if(!r){
		return NULL;
	}
	hs->seek = r->key;
	return r->key;
}


////////////////////////////////////////////////////
//キー名から値の構造体を取得
struct hdr *vmodreq_getrawheader(struct vmod_request *c, enum VMODREQ_TYPE type, const char *header)
{
	struct hdr *h;
	struct hdr *r = NULL;

//	int i=0;
	struct vmod_headers *hs;
	hs = vmodreq_getheaders(c,type);
	VTAILQ_FOREACH(h, &hs->headers, list) {
//		++i;
		if(!h->key && strcasecmp("", header) == 0){
			r = h;
			break;
		}else if (strcasecmp(h->key, header) == 0) {
			r = h;
			break;
		}
	}
	return r;
}

////////////////////////////////////////////////////
//値のサイズを取得
int vmodreq_getheadersize(struct vmod_request *c, enum VMODREQ_TYPE type, const char *header)
{
	struct hdr *h;
	int r = 0;
	h = vmodreq_getrawheader(c,type,header);
	
	if(h) r = h->size;
	
	return r;
}


////////////////////////////////////////////////////
//値を取得
const char *vmodreq_getheader(struct vmod_request *c, enum VMODREQ_TYPE type, const char *header)
{
	struct hdr *h;
	char *r = NULL;
	h = vmodreq_getrawheader(c,type,header);
	
	if(h) r = h->value;
	
	return r;
}


////////////////////////////////////////////////////
//サイズを取得（呼び出し用にラップ）
int vmodreq_headersize(struct sess *sp, enum VMODREQ_TYPE type, const char *header)
{
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	return vmodreq_getheadersize(c,type,header);
}


////////////////////////////////////////////////////
//値を取得（呼び出し用にラップ）
const char *vmodreq_header(struct sess *sp, enum VMODREQ_TYPE type, const char *header)
{
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	return vmodreq_getheader(c,type,header);
}

//////////////////////////////////////////
//フック時に呼び出される関数
//hook function(miss,pass,pipe)
static int vmod_Hook_unset_bereq(struct sess *sp){
	VRT_SetHdr(sp, HDR_BEREQ, POST_REQ_HDR, 0);
	
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
//フック時に呼び出される関数（deliver)
//hook function(deliver)
static int vmod_Hook_unset_deliver(struct sess *sp){
	int ret = vmod_Hook_deliver(sp);
	struct vmod_request *c;
	c = vmodreq_get_raw(sp);
	if(c)
		vmodreq_free(c);
	debugmsg(sp,"vmprd %d <<<<<<<<<<<<<<<end",sp->xid);

	return(ret);

}

//////////////////////////////////////////
//文字列から列挙型VMODREQ_TYPEに変換
enum VMODREQ_TYPE vmod_convtype(const char*type){
	if (!strcmp(type, "post"))
		return POST;
	if (!strcmp(type, "get"))
		return GET;
	if (!strcmp(type, "cookie"))
		return COOKIE;
	if (!strcmp(type, "req"))
		return REQ;
}

//////////////////////////////////////////
//文字列から列挙型gethdr_eに変換
enum gethdr_e vmod_convhdrtype(const char*type, unsigned* ret){
	*ret = (1==1);
	if (!strcmp(type, "req"))
		return HDR_REQ;
	if (!strcmp(type, "beresp"))
		return HDR_BERESP;
	if (!strcmp(type, "obj"))
		return HDR_OBJ;
	if (!strcmp(type, "bereq"))
		return HDR_BEREQ;
	if (!strcmp(type, "resp"))
		return HDR_RESP;
	*ret = (1==0);
	//return NULL;
}

//////////////////////////////////////////
//GetHdr用の文字列を作る
void gen_hdrtxt(const char *header,char *p, int size){
	int len = strlen(header);
	if(size < len + 3 && len > 252){
		*p = NULL;
		return;
	}
	p[0] = (unsigned char)len +1;
	++p;
	memcpy(p, header, len);
	p += len;
	p[0] = ':';
	p[1] = 0;
}
//////////////////////////////////////////
//vrt.cから移植
struct http *
vrt_selecthttp(const struct sess *sp, enum gethdr_e where)
{
	struct http *hp;

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	switch (where) {
	case HDR_REQ:
		hp = sp->http;
		break;
	case HDR_BEREQ:
		hp = sp->wrk->bereq;
		break;
	case HDR_BERESP:
		hp = sp->wrk->beresp;
		break;
	case HDR_RESP:
		hp = sp->wrk->resp;
		break;
	case HDR_OBJ:
		CHECK_OBJ_NOTNULL(sp->obj, OBJECT_MAGIC);
		hp = sp->obj->http;
		break;
	default:
		INCOMPL();
	}
	CHECK_OBJ_NOTNULL(hp, HTTP_MAGIC);
	return (hp);
}
//////////////////////////////////////////
//ヘッダの個数を取得
int count_header(const struct sess *sp, enum gethdr_e where)
{
	struct http *hp;
	hp = vrt_selecthttp(sp, where);
	return hp->nhd - HTTP_HDR_FIRST;
}

const char *get_header_key(const struct sess *sp, enum gethdr_e where, int index){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);

	if(index == 0) return NULL;
	int len = count_header(sp, where);
	if(len < index) return NULL;
	index += HTTP_HDR_FIRST -1;

	struct http *hp;
	hp = vrt_selecthttp(sp, where);
	const char *p = hp->hd[index].b;
	char * p2 = strchr(p,':');
	unsigned l = p2 - p;
	if(l > 255) return NULL;
	memcpy(c->seek_tmp, p, l);
	c->seek_tmp[l] = 0;
	return c->seek_tmp;
}
void init_header(const struct sess *sp, enum gethdr_e where){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	struct vmod_headers *h;
	int *en;
	switch (where) {
		case HDR_REQ:
			h = c->hdr_req;
			en = &c->init_req;
			break;
	}
	if(*en){
		//初期化必要
		vmodreq_headers_free(h);
		ALLOC_OBJ(h,VMOD_HEADERS_MAGIC);
		AN(h);
	}
	int max = count_header(sp,where);
	char *tmpkey;
	struct hdr *vh;
	
	for(int i=1; i<=max; i++){
		tmpkey = get_header_key(sp, where, i);
		vh = calloc(1, sizeof(struct hdr));
		AN(vh);
		vh->key = strndup(tmpkey, strlen(tmpkey));
		AN(vh->key);
		VTAILQ_INSERT_HEAD(&h->headers, vh, list);
	}
	*en = (1==1);

}
//////////////////////////////////////////
//3.0.3からインタフェースが変更されたHTC_Readの対策
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
	}
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

//////////////////////////////////////////
//content-typeがマルチパートのデータをパースする
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

	struct vmod_request *c = vmodreq_get(sp);
	int u;
	char *orig_cv_body, *cv_body, *h_val;
	
	//////////////////////////////
	//get boundary

	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	raw_boundary = strstr(h_ctype_ptr,"; boundary=");
	if(!raw_boundary || strlen(raw_boundary) > 255){
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
//		tmp2          = p_body_end[0];
		p_body_end[0] = 0;
		//bodyをURLエンコードする

	
		vmodreq_sethead(c,POST,sc_name,start_body,p_body_end - start_body);		
		
		name_line_end[idx]		= tmp;
//		p_body_end[0]			= tmp2;

		///////////////////////////////////////////////
		//check last boundary
		if(!p_end) break;
		
		p_start = p_end;
	}

	return 1;
}

//////////////////////////////////////////
//URLエンコードされてるデータをパースする
int vmodreq_decode_urlencode(struct sess *sp,char *body,enum VMODREQ_TYPE type,char eq,char amp,int size){
	
	char *sc_eq,*sc_amp;
	char *tmpbody = body;
	char* tmphead;
	char tmp,tmp2;
	struct vmod_request *c = vmodreq_get(sp);
	char *start;

	while(1){
		if(size<0) break;
		start = tmpbody;
		if(!tmpbody[0] || strlen(tmpbody)==0) break;
//		sc_eq = strchr(tmpbody,eq);
//		sc_amp = strchr(tmpbody,amp);
		sc_eq = memchr(tmpbody,eq,size);
		sc_amp = memchr(tmpbody,amp,size);
		//////////////////////////////
		//search word
		if(sc_amp < sc_eq && sc_amp){
			
			sc_eq=NULL;
		}
		if(!sc_eq && !sc_amp){
			vmodreq_sethead(c,type,tmpbody,"",0);
			break;
		}else if(!sc_eq){
			tmp2 = sc_amp[0];
			sc_amp[0] = 0;// & -> null
			vmodreq_sethead(c,type,tmpbody,"",0);
			sc_amp[0] = tmp2;
			tmpbody =sc_amp+1;
			size -= tmpbody - start;
			continue;
		}
		if(!sc_amp){
			
			//sc_amp = sc_eq + strlen(tmpbody);
			sc_amp = tmpbody + size;
		}
		
		//////////////////////////////
		//build head
		tmp=sc_eq[0];
		sc_eq[0] = 0;// = -> null
		

		tmphead = tmpbody;
		

		//////////////////////////////
		//build body
		tmpbody   = sc_eq + 1;

		//////////////////////////////
		//set header
		
		vmodreq_sethead(c,type,tmphead,tmpbody,sc_amp - tmpbody);
		
		sc_eq[0]  = tmp;//thead
		tmpbody   = sc_amp + 1;
		while(1){
			//for COOKIE (Sometimes it contains spaces at key prefix.)
			if(tmpbody[0]==0) break;
			if(tmpbody[0] ==' '){
				++tmpbody;
			}else{
				break;
			}
		}
		size -= tmpbody - start;

	}
	return 1;

}


//////////////////////////////////////////
//クッキーをパース
int vmodreq_cookie_parse(struct sess *sp){
	struct vmod_request *c = vmodreq_get(sp);
	if(!c->raw_cookie) return 1;

	return vmodreq_decode_urlencode(sp,c->raw_cookie,COOKIE,'=',';',c->size_cookie);
}

//////////////////////////////////////////
//GETをパース
int vmodreq_get_parse(struct sess *sp){
	int ret=1;
	struct vmod_request *c = vmodreq_get(sp);
	if(c->raw_get)
		ret = vmodreq_decode_urlencode(sp,c->raw_get,GET,'=','&',c->size_get);
	return ret;
}



//////////////////////////////////////////
//リクエストBodyを取得
int vmodreq_reqbody(struct sess *sp, char**body,int *orig_content_length){
	debugmsg(sp,"vmprd %d vmodreq_reqbody|start",sp->xid);
	unsigned long		content_length;
	char 				*h_clen_ptr;
	int					buf_size, rsize, re;
	char				buf[2048];
	enum VMODREQ_PARSE	exec;

	//////////////////////////////
	//check Content-Length
	h_clen_ptr = VRT_GetHdr(sp, HDR_REQ, "\017Content-Length:");
	if (!h_clen_ptr) {
		//can't get
		return -2;
	}
	*orig_content_length = content_length = strtoul(h_clen_ptr, NULL, 10);
	debugmsg(sp,"vmprd %d vmodreq_reqbody|content-length %ld",sp->xid,content_length);
	if (content_length <= 0) {
		//illegal length
		return -2;
	}
	//////////////////////////////
	//Check POST data is loaded
	if(sp->htc->pipeline.b != NULL && Tlen(sp->htc->pipeline) == content_length){
		//complete read
		debugmsg(sp,"vmprd %d vmodreq_reqbody|compete read",sp->xid);
		*body = sp->htc->pipeline.b;
	}else{
		//incomplete read
		debugmsg(sp,"vmprd %d vmodreq_reqbody|incompete read",sp->xid);
		int rxbuf_size = Tlen(sp->htc->rxbuf);
		debugmsg(sp,"vmprd %d vmodreq_reqbody|rxbufsize len:%d",sp->xid,rxbuf_size);
		///////////////////////////////////////////////
		//use ws
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < content_length + rxbuf_size + 1){
			debugmsg(sp,"vmprd %d vmodreq_reqbody|none space(-1)",sp->xid);
			WS_Release(sp->wrk->ws,0);
			return -1;
		}
		*body = (char*)sp->wrk->ws->f;
		memcpy(*body, sp->htc->rxbuf.b, rxbuf_size);
		sp->htc->rxbuf.b = *body;
		*body            += rxbuf_size;
		*body[0]         = 0;
		sp->htc->rxbuf.e = *body;
		WS_Release(sp->wrk->ws,content_length + rxbuf_size + 1);
		///////////////////////////////////////////////
		debugmsg(sp,"vmprd %d vmodreq_reqbody|old pipeline e:%ld b:%ld len:%ld",sp->xid,sp->htc->pipeline.b ,sp->htc->pipeline.e, sp->htc->pipeline.e - sp->htc->pipeline.b);
		//////////////////////////////
		//read post data
		re = 0;
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
			debugmsg(sp,"vmprd %d vmodreq_reqbody|HTC_Read len:%d",sp->xid,rsize);
			if (rsize <= 0) {
				return -3;
			}

			content_length -= rsize;

			memcpy(*body + re, buf, buf_size);
			
			//strncat(*body, buf, buf_size);
			re += rsize;
		}
		
		sp->htc->pipeline.b = *body;
		sp->htc->pipeline.e = *body + *orig_content_length;
		debugmsg(sp,"vmprd %d vmodreq_reqbody|new pipeline e:%ld b:%ld len:%ld",sp->xid,sp->htc->pipeline.b ,sp->htc->pipeline.e, sp->htc->pipeline.e - sp->htc->pipeline.b);

	}
	return 1;
}

//////////////////////////////////////////
//POSTをパース
int vmodreq_post_parse(struct sess *sp){
	debugmsg(sp,"vmprd %d >>>>>>>>>>>>>>>start",sp->xid);

/*
	2	=sucess		unknown or none content-type
	1	=sucess
	-1	=error		less sess_workspace
	-2	=error		none content-length key
	-3	=error		less content-length
*/	
	char 				*h_ctype_ptr, *body;
	int					ret,content_length;
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
	debugmsg(sp,"vmprd %d vmodreq_post_parse|content-type|%d",sp->xid,exec);
	

	
	//get request body
	ret = vmodreq_reqbody(sp,&body,&content_length);
	if(ret<1 && exec != UNKNOWN) return ret;

	//decode form
	switch(exec){
		case URL:
			ret = vmodreq_decode_urlencode(sp,body,POST,'=','&',content_length);
			break;
		case MULTI:
			ret = decodeForm_multipart(sp, body);
			break;
		case UNKNOWN:
			ret = 2;
			break;
		
	}
	return ret;

}

//////////////////////////////////////////
//デバッグ用
void debugmsg(struct sess *sp,const char* f,...){
	if(!is_debug) return;
	va_list ap;
	va_start(ap, f);
	vsyslog(LOG_NOTICE, f ,ap);
	va_end(ap);
}
