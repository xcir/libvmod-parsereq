#include "vmod_parsereq.h"



///////////////////////////////////////////////////////////////
//キーのオフセット初期化
void vmod_reset_offset(struct sess *sp, const char* type){
	vmodreq_seek_reset(sp,vmod_convtype(type));
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
	return vmodreq_seek(sp,vmod_convtype(type));
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
	vmodreq_seek(sp,vmod_convtype(type));
}
/*
void vmod_post_read_next(struct sess *sp){
	vmodreq_seek(sp,POST);
}

void vmod_get_read_next(struct sess *sp){
	vmodreq_seek(sp,GET);
}

void vmod_cookie_read_next(struct sess *sp){
	vmodreq_seek(sp,COOKIE);
}
*/


///////////////////////////////////////////////////////////////
//現在のキー名を取得
const char* vmod_current_key(struct sess *sp, const char* type){
	return vmod_read_cur(sp,vmod_convtype(type));
}
/*
const char* vmod_post_read_cur(struct sess *sp){
	return vmod_read_cur(sp,POST);
}
const char* vmod_get_read_cur(struct sess *sp){
	return vmod_read_cur(sp,GET);
}
const char* vmod_cookie_read_cur(struct sess *sp){
	return vmod_read_cur(sp,COOKIE);
}
*/

///////////////////////////////////////////////////////////////
//反復処理系
void vmod_iterate(struct sess *sp, const char* type, const char* p){
	vmod_read_iterate(sp,p,vmod_convtype(type));
}
/*
void vmod_post_read_iterate(struct sess *sp, const char* p){
	vmod_read_iterate(sp,p,POST);
}
void vmod_get_read_iterate(struct sess *sp, const char* p){
	vmod_read_iterate(sp,p,GET);
}
void vmod_cookie_read_iterate(struct sess *sp, const char* p){
	vmod_read_iterate(sp,p,COOKIE);
}
*/


///////////////////////////////////////////////////////////////
//サイズ取得系関数
int vmod_size(struct sess *sp, const char *type, const char *header)
{
	return vmodreq_headersize(sp,vmod_convtype(type) ,header);
}
/*
int vmod_post_size(struct sess *sp, const char *header)
{
	return vmodreq_headersize(sp,POST,header);
}
int vmod_get_size(struct sess *sp, const char *header)
{
	return vmodreq_headersize(sp,GET,header);
}

int vmod_cookie_size(struct sess *sp, const char *header)
{
	return vmodreq_headersize(sp,COOKIE,header);
}
*/

///////////////////////////////////////////////////////////////
//Value取得系関数
const char *vmod_param(struct sess *sp, const char *type ,const char *header){
	return vmodreq_header(sp,vmod_convtype(type) ,header);
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
