/* SPDX-License-Identifier: MIT */
/* Include the actual implementation to exercise its private cache seam. Link
 * with section garbage collection: no server/main/network path is invoked. */
#include "../src/rtsp/rtp.c"

static unsigned checks, failures;
#define EXPECT(x) do { ++checks; if (!(x)) { ++failures; \
    fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); } } while (0)

static void init(struct __rtsp_obj_t *h,char hevc) {
    memset(h,0,sizeof(*h));h->isH265=hevc;
    EXPECT(pthread_mutex_init(&h->mutex,NULL)==0);
}
static void finish(struct __rtsp_obj_t *h) {
    mime_encoded_delete(h->sprop_vps_b64);mime_encoded_delete(h->sprop_sps_b64);
    mime_encoded_delete(h->sprop_pps_b64);mime_encoded_delete(h->sprop_sps_b16);
    EXPECT(pthread_mutex_destroy(&h->mutex)==0);
}
static void encoded(mime_encoded_handle value,const char *literal,size_t length) {
    EXPECT(value!=NULL);
    if(value) {
        EXPECT(value->base==64 && value->len_src==length);
        EXPECT(value->len_result==strlen(literal)+1);
        EXPECT(strcmp(value->result,literal)==0);
    }
}
int main(int argc,char **argv) {
    struct __rtsp_obj_t h;unsigned type,prefix;
    if(argc==2 && strcmp(argv[1],"--short-vps-two")==0) {
        /* Corrected-code guard check: exactly two payload bytes in this
         * stack object. The expected behavior is safe failure. */
        unsigned char two[]={0,0,1,0x40,1};int status;
        init(&h,1);status=__retrieve_sprop(&h,two,sizeof(two));
        EXPECT(status==FAILURE && h.sprop_vps_b64==NULL);finish(&h);
        printf("short VPS checks=%u failures=%u\n",checks,failures);
        return failures?1:0;
    }
    EXPECT(argc==1);
    /* Synthetic fixed-header examples, not captured/decoder-valid video.
     * Exact encodings are independent literal oracle values. */
    unsigned char hevc[]={0,0,0,1,0x40,1,0xab,0xcd,
        0,0,1,0x42,1,0xab,0xcd,0,0,0,1,0x44,1,0xab,0xcd};
    unsigned char avc[]={0,0,1,0x67,0x42,0,0x1f,0,0,0,1,0x68,0xce,0x38,0x80};
    init(&h,1);
    EXPECT(__retrieve_sprop(&h,hevc,sizeof(hevc))==SUCCESS);
    encoded(h.sprop_vps_b64,"QAGrzQ==",4);
    encoded(h.sprop_sps_b64,"QgGrzQ==",4);
    encoded(h.sprop_pps_b64,"RAGrzQ==",4);
    EXPECT(h.sprop_sps_b16!=NULL);
    if(h.sprop_sps_b16) EXPECT(strcmp(h.sprop_sps_b16->result,"01ABCD")==0);
    /* Preserve existing first-value cache behavior across a later input. */
    {
        mime_encoded_handle first=h.sprop_vps_b64;
        unsigned char changed[]={0,0,1,0x40,1,0x11,0x22};
        EXPECT(__retrieve_sprop(&h,changed,sizeof(changed))==SUCCESS);
        EXPECT(h.sprop_vps_b64==first);
        encoded(h.sprop_vps_b64,"QAGrzQ==",4);
    }
    finish(&h);
    init(&h,0);
    EXPECT(__retrieve_sprop(&h,avc,sizeof(avc))==SUCCESS);
    EXPECT(h.sprop_vps_b64==NULL);
    encoded(h.sprop_sps_b64,"Z0IAHw==",4);
    encoded(h.sprop_pps_b64,"aM44gA==",4);
    EXPECT(h.sprop_sps_b16!=NULL);
    if(h.sprop_sps_b16) EXPECT(strcmp(h.sprop_sps_b16->result,"42001F")==0);
    finish(&h);
    /* The actual cache predicate must accept only type32 as HEVC VPS; SPS,
     * PPS, adjacent/other types and the low layer-id bit are independent. */
    for(prefix=3;prefix<=4;++prefix) for(type=0;type<64;++type) {
        unsigned layer;
        for(layer=0;layer<2;++layer) {
            unsigned char bytes[8]={0};bytes[prefix-1]=1;
            bytes[prefix]=(unsigned char)((type<<1)|layer);
            bytes[prefix+1]=1;bytes[prefix+2]=0xab;bytes[prefix+3]=0xcd;
            init(&h,1);
            EXPECT(__retrieve_sprop(&h,bytes,prefix+4)==SUCCESS);
            EXPECT((h.sprop_vps_b64!=NULL)==(type==32));
            EXPECT((h.sprop_sps_b64!=NULL)==(type==33));
            EXPECT((h.sprop_pps_b64!=NULL)==(type==34));
            finish(&h);
        }
    }
    for(type=0;type<32;++type) {
        unsigned char bytes[]={0,0,1,0x60,0x41,0xab,0xcd};
        bytes[3]|=(unsigned char)type;init(&h,0);
        EXPECT(__retrieve_sprop(&h,bytes,sizeof(bytes))==SUCCESS);
        EXPECT(h.sprop_vps_b64==NULL);
        EXPECT((h.sprop_sps_b64!=NULL)==(type==7));
        EXPECT((h.sprop_pps_b64!=NULL)==(type==8));
        finish(&h);
    }
    /* A newly enabled VPS path must reject short payloads before MIME encoding.
     * Fresh cache only: this does not change the existing already-cached policy. */
    for(prefix=3;prefix<=4;++prefix) {
        unsigned length;
        for(length=1;length<=3;++length) {
            unsigned char bytes[7]={0};bytes[prefix-1]=1;bytes[prefix]=0x40;
            if(length>1) bytes[prefix+1]=1;
            if(length>2) bytes[prefix+2]=0xab;
            init(&h,1);
            EXPECT(__retrieve_sprop(&h,bytes,prefix+length)==FAILURE);
            EXPECT(h.sprop_vps_b64==NULL && h.sprop_sps_b64==NULL && h.sprop_pps_b64==NULL);
            finish(&h);
        }
    }
    /* Actual splitter returns no segment for empty/bare-prefix inputs; the
     * caller must not read a header byte when no segment was produced. */
    for(prefix=0;prefix<=4;++prefix) {
        unsigned char bytes[4]={0,0,0,1};unsigned char *nal=bytes;size_t len=0;
        if(prefix==1 || prefix==2) continue;
        if(prefix==3) bytes[2]=1;
        EXPECT(nal_split(bytes,&nal,&len,prefix)==FAILURE);
        init(&h,1);EXPECT(__retrieve_sprop(&h,bytes,prefix)==SUCCESS);
        EXPECT(h.sprop_vps_b64==NULL && h.sprop_sps_b64==NULL && h.sprop_pps_b64==NULL);
        finish(&h);
    }
    printf("actual RTSP cache checks=%u failures=%u\n",checks,failures);
    return failures?1:0;
}
