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

static int type_htcread = 0;

//#define DEBUG_HTCREAD
//#define DEBUG_SYSLOG

//HTC_Read ~3.0.3
typedef ssize_t HTC_READ302(struct http_conn *htc, void *d, size_t len);

//HTC_Read 3.0.3~
typedef ssize_t HTC_READ303(struct worker *w, struct http_conn *htc, void *d, size_t len);

int
init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
	return (0);
}

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


unsigned __url_encode(char* url,int *calc,char *copy){
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
unsigned url_encode_setHdr(struct sess *sp, char* url,char *head,int *encoded_size){
	char *copy;
	char buf[3075];
	int size = 3 * strlen(url) + 3;
	if(size > 3075){
		///////////////////////////////////////////////
		//use ws
		int u = WS_Reserve(sp->wrk->ws, 0);
		if(u < size){
			return 0;
		}
		copy = (char*)sp->wrk->ws->f;
		///////////////////////////////////////////////
	}else{
		copy = buf;
	}
	__url_encode(url,encoded_size,copy);

	*encoded_size += strlen(copy) + head[0] +1;
	if(size > 3075){
		WS_Release(sp->wrk->ws,strlen(copy)+1);
	}
	//sethdr
    VRT_SetHdr(sp, HDR_REQ, head, copy, vrt_magic_string_end);
	return(1);
}

void decodeForm_multipart(struct sess *sp,char *body,char *tgHead,unsigned parseFile,const char *paramPrefix, unsigned setParam){
	
	char *prefix;
	char defPrefix[] = "X-VMODPOST-";
	if(!setParam){
		prefix = defPrefix;
	}else{
		prefix = (char*)paramPrefix;
	}
	int prefix_len = strlen(prefix);
	char tmp;
	char head[256];
	char sk_boundary[258];
	char *raw_boundary, *h_ctype_ptr;
	char *p_start, *p_end, *p_body_end, *name_line_end, *name_line_end_tmp, *start_body, *sc_name, *sc_filename;
	int  hsize, boundary_size ,idx;
	
	char *tmpbody    = body;
	int  encoded_size= 0;
	char *basehead   = 0;
	char *curhead;
	int  ll_u, ll_head_len, ll_size;
	int  ll_entry_count = 0;

	int u;
	char *orig_cv_body, *cv_body, *h_val;
	
	//////////////////////////////
	//get boundary
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	raw_boundary = strstr(h_ctype_ptr,"; boundary=");
	if(!raw_boundary || strlen(raw_boundary) > 255) return;
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
			idx=0;
		}
		tmp                   = name_line_end[idx];
		name_line_end[idx]    = 0;
		hsize                 = strlen(sc_name) + prefix_len +1;
		if(hsize > 255) break;
		head[0]               = hsize;
		head[1]               = 0;
		snprintf(head +1,255,"%s%s:",prefix,sc_name);
		name_line_end[idx]    = tmp;
		

		///////////////////////////////////////////////
		//filecheck
		if(!parseFile){
			if(sc_filename && sc_filename < start_body){
				//content is file(skip)
				p_start = p_end;
				continue;
			}
		}
		
		///////////////////////////////////////////////
		//create linked list
		//use ws
		ll_u = WS_Reserve(sp->wrk->ws, 0);
		ll_head_len = strlen(head +1) +1;
		ll_size = ll_head_len + sizeof(char*)+2;
		if(ll_u < ll_size){
			return ;
		}

		if(!basehead){
			curhead = (char*)sp->wrk->ws->f;
			basehead = curhead;
		}else{
			((char**)curhead)[0] = (char*)sp->wrk->ws->f;
			curhead = (char*)sp->wrk->ws->f;
		}
		++ll_entry_count;
		
		memcpy(curhead,head,ll_head_len);
		curhead +=ll_head_len;
		memset(curhead,0,sizeof(char*)+1);
		++curhead;
		WS_Release(sp->wrk->ws,ll_size);
		///////////////////////////////////////////////
		
		///////////////////////////////////////////////
		//url encode & set header
		p_body_end    = p_end -2;
		start_body    +=4;
		tmp           = p_body_end[0];
		p_body_end[0] = 0;
		//bodyをURLエンコードする
		if(!url_encode_setHdr(sp,start_body,head,&encoded_size)){
			//メモリない
			p_body_end[0] = tmp;
			break;
		}
		encoded_size -= prefix_len;
		p_body_end[0] = tmp;

		///////////////////////////////////////////////
		//check last boundary
		if(!p_end) break;
		
		p_start = p_end;
	}
	if(tgHead[0] == 0 && !setParam) return;

	//////////////
	//genarate x-www-form-urlencoded format data
	
	//////////////
	//use ws
	if(tgHead[0] == 0){
		orig_cv_body = cv_body = NULL;
	}else{
		u = WS_Reserve(sp->wrk->ws, 0);
		if(u < encoded_size + 1){
			return;
		}
		orig_cv_body = cv_body = (char*)sp->wrk->ws->f;
		WS_Release(sp->wrk->ws,encoded_size+1);
	}
	//////////////
	for(int i=0;i<ll_entry_count;++i){
		if(cv_body){
			hsize = basehead[0] -prefix_len;
			memcpy(cv_body,basehead+1+prefix_len,hsize);
			cv_body    += hsize - 1;
			cv_body[0] ='=';
			++cv_body;
			h_val      = VRT_GetHdr(sp,HDR_REQ,basehead);
		}
		if(!setParam){
			//remove header
			VRT_SetHdr(sp, HDR_REQ, basehead, 0);
		}
		if(cv_body){
			memcpy(cv_body,h_val,strlen(h_val));
			cv_body    +=strlen(h_val);
			cv_body[0] ='&';
			++cv_body;
		}
		basehead   = *(char**)(basehead+strlen(basehead+1)+2);
	}
	if(cv_body){
		orig_cv_body[encoded_size - 1] =0;
	}

	if(tgHead[0] == 0) return;
	VRT_SetHdr(sp, HDR_REQ, tgHead, orig_cv_body, vrt_magic_string_end);
	
}
void decodeForm_urlencoded(struct sess *sp,char *body,const char *paramPrefix){
	char head[256];
	char *sc_eq,*sc_amp;
	char *tmpbody = body;
	int hsize;
	char tmp;
	int prefix_len = strlen(paramPrefix);
	
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
		tmp = sc_eq[0];
		sc_eq[0] = 0;// = -> null
		
		hsize = strlen(tmpbody) + prefix_len + 1;
		if(hsize > 255) return;
		head[0]   = hsize;
		head[1]   = 0;
		snprintf(head +1,255,"%s%s:", paramPrefix, tmpbody);
		sc_eq[0]  = tmp;

		//////////////////////////////
		//build body
		tmpbody   = sc_eq + 1;
		tmp       = sc_amp[0];
		sc_amp[0] = 0;// & -> null

		//////////////////////////////
		//set header
		VRT_SetHdr(sp, HDR_REQ, head, tmpbody, vrt_magic_string_end);
		sc_amp[0] = tmp;
		tmpbody   = sc_amp + 1;
	}
}
int 
vmod_parse(struct sess *sp,const char* tgHeadName,unsigned setParam,const char* paramPrefix,unsigned parseMulti,unsigned parseFile){

/*
	struct sess *sp,			OK
	const char* tgHeadName,		OK
	const char* paramPrefix,	url,
	unsigned setParam,			url,
	unsigned parseMulti,		OK
	unsigned parseFile			OK
*/	
	//デバッグでReInitとかrestartの時に不具合でないかチェック（ロールバックも）
//Content = pipeline.e-bの時はRxbuf確保をしない（必要ないので）
//mix形式をurlencodeに切り替える（組み換えで安全に）<-完了
/*
	  string(41) "submitter\"=a&submitter2=b&submitter3=vcc"
　　  string(42) "submitter%5C=a&submitter2=b&submitter3=vcc"
　　  
  string(39) "submitter=a&submitter2=b&submitter3=vcc"
  string(39) "submitter=a&submitter2=b&submitter3=vcc"
  string(83) "submitter=%e3%81%82%e3%81%84%e3%81%86%e3%81%88%e3%81%8a&submitter2=b&submitter3=vcc"
  string(83) "submitter=%E3%81%82%E3%81%84%E3%81%86%E3%81%88%E3%81%8A&submitter2=b&submitter3=vcc"

	1	=成功
	-1	=エラー		ワークスペースサイズが足りない
	-2	=エラー		target/ContentLengthがない/不正
	-3	=エラー		指定ContentLengthに満たないデータ
	-4	=エラー		未対応形式
	if (!(
		!VRT_strcmp(VRT_r_req_request(sp), "POST") ||
		!VRT_strcmp(VRT_r_req_request(sp), "PUT")
	)){return "";}
*/
	unsigned long	content_length,orig_content_length;
	char 			*h_clen_ptr, *h_ctype_ptr, *body;
	int				buf_size, rsize;
	char			buf[1024],tgHead[256];
	unsigned		multipart = 0;

	
	
	//////////////////////////////
	//build tgHead
	int hsize = strlen(tgHeadName) +1;
	if(hsize > 1){
		if(hsize > 255) return -2;
		tgHead[0] = hsize;
		tgHead[1] = 0;
		snprintf(tgHead +1,255,"%s:",tgHeadName);
	}else{
		tgHead[0] = 0;
	}

	//////////////////////////////
	//check Content-Type
	h_ctype_ptr = VRT_GetHdr(sp, HDR_REQ, "\015Content-Type:");
	
	if (!VRT_strcmp(h_ctype_ptr, "application/x-www-form-urlencoded")) {
		//application/x-www-form-urlencoded
	}else if(h_ctype_ptr != NULL && h_ctype_ptr == strstr(h_ctype_ptr, "multipart/form-data") && parseMulti){
		//multipart/form-data
		multipart = 1;
	}else{
		//none support type
		return -4;
	}

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
#ifdef DEBUG_HTCREAD
	if(0 == 1){
#else
	if(sp->htc->pipeline.b != NULL && Tlen(sp->htc->pipeline) == content_length){
#endif		
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

			hsize += rsize;
			content_length -= rsize;

			strncat(body, buf, buf_size);
		}
		sp->htc->pipeline.b = body;
		sp->htc->pipeline.e = body + orig_content_length;
	}


	//////////////////////////////
	//decode form
	if(multipart){
		decodeForm_multipart(sp, body,tgHead,parseFile,paramPrefix,setParam);
	}else{
		if(tgHead[0] != 0)
			VRT_SetHdr(sp, HDR_REQ, tgHead, body, vrt_magic_string_end);
		if(setParam)
			decodeForm_urlencoded(sp, body,paramPrefix);
	}
	return 1;
}
