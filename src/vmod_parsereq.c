#include "vmod_parsereq.h"

/*
	todo:
		urlencode系でのsetheader系がカオスなので見直す
		vmodreq_setheadがカオスってる
		リビルドをするときはseekをNULLにリセット
*/
//////////////////////////////////////////
//VMOD
int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

////////////////////////////////////////////////////
//get header value

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


const char *vmod_param(struct sess *sp, const char *type ,const char *header){
	return vmodreq_header(sp,vmod_convtype(type) ,header);
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
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	return c->raw_post;
}
const char* vmod_get_body(struct sess *sp){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
	return c->raw_get;
}
const char* vmod_cookie_body(struct sess *sp){
	if(!vmodreq_get_raw(sp)){
		VRT_panic(sp,"please write \"parsereq.init();\" to 1st line in vcl_recv.",vrt_magic_string_end);
	}
	struct vmod_request *c = vmodreq_get(sp);
//	syslog(6,"ckck [%s] %d",c->raw_cookie,strlen(c->raw_cookie));

	return c->raw_cookie;
}


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

