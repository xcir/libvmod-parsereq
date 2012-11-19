#include "vmod_parsereq.h"



///////////////////////////////////////////////////////////////
//キーのオフセット初期化
void vmod_reset_offset(struct sess *sp, const char* type){
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		header_idx_reset(sp, where);
	}else{
		vmodreq_seek_reset(sp,vmod_convtype(type));
	}
}

void vmod_post_seek_reset(struct sess *sp){
	vmodreq_seek_reset(sp,POST);
}
void vmod_get_seek_reset(struct sess *sp){
	vmodreq_seek_reset(sp,GET);
}
void vmod_cookie_seek_reset(struct sess *sp){
	vmodreq_seek_reset(sp,COOKIE);
}

///////////////////////////////////////////////////////////////
//キーのオフセットを移動＋移動後の値取得
const char *vmod_next_key(struct sess *sp, const char *type){
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		return header_next(sp, where);
	}else{
		return vmodreq_seek(sp,vmod_convtype(type));
	}
}

const char* vmod_get_read_keylist(struct sess *sp){
	return vmodreq_seek(sp,GET);
}
const char* vmod_post_read_keylist(struct sess *sp){
	return vmodreq_seek(sp,POST);
}
const char* vmod_cookie_read_keylist(struct sess *sp){
	return vmodreq_seek(sp,COOKIE);
}



///////////////////////////////////////////////////////////////
//キーのオフセットを移動
void vmod_next_offset(struct sess *sp, const char *type){
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		header_next(sp, where);
	}else{
		vmodreq_seek(sp,vmod_convtype(type));
	}
	
}

///////////////////////////////////////////////////////////////
//現在のキー名を取得
const char* vmod_current_key(struct sess *sp, const char* type){
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		if(!vmodreq_get_raw(sp)){
			VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
		}
		struct vmod_request *c = vmodreq_get(sp);
		switch (where) {
		case HDR_REQ:
			return get_header_key(sp, where, c->seek_idx_req);
			break;
		case HDR_BEREQ:
			return get_header_key(sp, where, c->seek_idx_bereq);
			break;
		case HDR_BERESP:
			return get_header_key(sp, where, c->seek_idx_beresp);
			break;
		case HDR_RESP:
			return get_header_key(sp, where, c->seek_idx_resp);
			break;
		case HDR_OBJ:
			return get_header_key(sp, where, c->seek_idx_obj);
			break;
		}
	}else{
		return vmod_read_cur(sp,vmod_convtype(type));
	}
}

///////////////////////////////////////////////////////////////
//反復処理系
void vmod_iterate(struct sess *sp, const char* type, const char* p){
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		header_iterate(sp, p, where);
	}else{
		vmod_read_iterate(sp,p,vmod_convtype(type));
	}
}


///////////////////////////////////////////////////////////////
//サイズ取得系関数
int vmod_size(struct sess *sp, const char *type, const char *header)
{
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		//headerの値を作る必要がある
		char tmp[256];
		gen_hdrtxt(header, &tmp, 256);
		return strlen(VRT_GetHdr(sp, where, tmp));
	}else{
		return vmodreq_headersize(sp,vmod_convtype(type) ,header);
	}
}

///////////////////////////////////////////////////////////////
//Value取得系関数
const char *vmod_param(struct sess *sp, const char *type ,const char *header){
	unsigned ret = 0;
	enum gethdr_e where = vmod_convhdrtype(type, &ret);
	if(ret){
		//headerの値を作る必要がある
		char tmp[256];
		gen_hdrtxt(header, &tmp, 256);
		return VRT_GetHdr(sp, where, tmp);
	}else{
		return vmodreq_header(sp,vmod_convtype(type) ,header);
	}
}

const char *vmod_post_header(struct sess *sp, const char *header){
	return vmodreq_header(sp,POST,header);
}

const char *vmod_get_header(struct sess *sp, const char *header){
	return vmodreq_header(sp,GET,header);
}

const char *vmod_cookie_header(struct sess *sp, const char *header){
	return vmodreq_header(sp,COOKIE,header);
}

///////////////////////////////////////////////////////////////
//生body取得系関数
const char* vmod_body(struct sess *sp, const char *type){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	enum VMODREQ_TYPE t = vmod_convtype(type);
	switch(t){
		case POST:
			return c->raw_post;
			break;
		case GET:
			return c->raw_get;
			break;
		case COOKIE:
			return c->raw_cookie;
			break;
	}

}

const char* vmod_post_body(struct sess *sp){
	return vmod_body(sp, "post");
}

const char* vmod_get_body(struct sess *sp){
	return vmod_body(sp, "get");
}

const char* vmod_cookie_body(struct sess *sp){
	return vmod_body(sp, "cookie");
}
///////////////////////////////////////////////////////////////
//初期化、デバッグなどシステム系
void vmod_init(struct sess *sp){
	vmodreq_get(sp);
}

void vmod_debuginit(struct sess *sp)
{
	is_debug = 1;
	vmodreq_get(sp);
}

int vmod_errcode(struct sess *sp){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	return vmodreq_get(sp)->parse_ret;
}

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}
