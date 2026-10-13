/* SPDX-License-Identifier: MIT */
/* Direct HAL input -> public packetizer. Socket send is captured; the unused
 * wall-clock fallback aborts if reached. No server, socket, device, capture
 * source or worker thread is created by this test. */
#include <sys/socket.h>
static ssize_t capture_send(int, const void *, size_t, int);
#define send capture_send
#include "../src/rtsp/rtp.c"
#undef send
#include "framing-fixtures/fixtures.h"

/* All cases provide positive capture timestamps. Do not silently substitute
 * a host wall clock if a regression unexpectedly selects the fallback. */
unsigned int millis(void) { abort(); }

static unsigned checks, failures, case_id;
static const char *case_name;
#define EXPECT(x) do { ++checks; if (!(x)) { ++failures; \
 fprintf(stderr,"FAIL %s line %d: %s\n",case_name,__LINE__,#x); } } while(0)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define MAX_PACKET 1600
#define MAX_PACKETS 96
static struct { size_t len; unsigned char data[MAX_PACKET]; } packets[MAX_PACKETS];
static size_t packet_count, send_calls, fail_call;
static FILE *capture;
static struct connection_item_t connection;
static struct transfer_item_t transfer;
static struct __rtsp_obj_t server;
static threadpool pool;
static shared_interface shared;
static struct global_state_t global;
static unsigned char input[65536], before[65536], reconstructed[65536];
static const struct fixture {
 const char *name; const unsigned char *bytes; size_t size;
 const struct fixture_nal *nals; size_t count; int hevc;
} fixtures[]={
 {"h264-small",h264_small,sizeof(h264_small),h264_small_nals,ARRAY_SIZE(h264_small_nals),0},
 {"h264-large",h264_large,sizeof(h264_large),h264_large_nals,ARRAY_SIZE(h264_large_nals),0},
 {"h265-small",hevc_small,sizeof(hevc_small),hevc_small_nals,ARRAY_SIZE(hevc_small_nals),1},
 {"h265-large",hevc_large,sizeof(hevc_large),hevc_large_nals,ARRAY_SIZE(hevc_large_nals),1}
};
static ssize_t capture_send(int fd,const void *data,size_t len,int flags) {
 ++send_calls;
 if(fd!=31337 || flags!=0 || len>MAX_PACKET || packet_count==MAX_PACKETS) abort();
 if(fail_call && send_calls==fail_call) { errno=EPIPE;return -1; }
 packets[packet_count].len=len;memcpy(packets[packet_count++].data,data,len);
 return (ssize_t)len;
}
static void *get_connection(int n) { return n==0?&connection:NULL; }
static void *get_transfer(int n) { return n==0?&transfer:NULL; }
static void init(void) {
 struct connection_item_t *got=NULL;
 memset(&server,0,sizeof(server));memset(&connection,0,sizeof(connection));
 memset(&transfer,0,sizeof(transfer));memset(&pool,0,sizeof(pool));
 memset(&shared,0,sizeof(shared));memset(&global,0,sizeof(global));
 EXPECT(pthread_mutex_init(&server.mutex,NULL)==0);
 EXPECT(pthread_mutex_init(&connection.write_mutex,NULL)==0);
 EXPECT(pthread_mutex_init(&global.mutex,NULL)==0);
 pool.sharedp=&shared;shared.gbl=&global;server.pool=&pool;
 server.con_pool=bufpool_create(1,get_connection,NULL,sizeof(connection));
 server.transfer_pool=bufpool_create(1,get_transfer,NULL,sizeof(transfer));
 if(!server.con_pool || !server.transfer_pool) abort();
 connection.pool=server.con_pool;transfer.pool=server.transfer_pool;
 transfer.list_entry.cleaner=__transfer_item_cleaner;
 EXPECT(bufpool_get_free(server.con_pool,&got)==SUCCESS && got==&connection);
 EXPECT(list_push(&server.con_list,&connection.list_entry)==SUCCESS);
 connection.con_state=__CON_S_PLAYING;connection.trans[0].server_port_rtp=5004;
 connection.trans[0].server_rtp_fd=31337;connection.trans[0].rtp_seq=65530;
 connection.trans[0].rtcp_tick=100000;connection.ssrc=0x12345678;
 server.stat.ts_offset=123; /* capture timestamp must govern actual packet. */
 packet_count=send_calls=fail_call=0;
}
static void finish(void) {
 EXPECT(server.transfer_pool->elems[0].ref_count==0);
 EXPECT(connection.pool->elems[0].ref_count==1);
 EXPECT(list_length(&server.transfer_pool->free_list)==1);
 server.con_list.list=NULL;
 EXPECT(bufpool_detach(server.con_pool,&connection)==SUCCESS);
 bufpool_delete(server.transfer_pool);bufpool_delete(server.con_pool);
 mime_encoded_delete(server.sprop_vps_b64);mime_encoded_delete(server.sprop_sps_b64);
 mime_encoded_delete(server.sprop_pps_b64);mime_encoded_delete(server.sprop_sps_b16);
 EXPECT(pthread_mutex_destroy(&server.mutex)==0);
 EXPECT(pthread_mutex_destroy(&connection.write_mutex)==0);
 EXPECT(pthread_mutex_destroy(&global.mutex)==0);
}
static uint32_t be32(const unsigned char *p) {
 return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
}
static void record_u32(uint32_t v) {
 unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};
 if(capture && fwrite(b,1,4,capture)!=4) abort();
}
static void record(void) {
 if(!capture)return;
 record_u32(case_id);record_u32((uint32_t)packet_count);
 for(size_t i=0;i<packet_count;i++) {
  record_u32((uint32_t)packets[i].len);
  if(fwrite(packets[i].data,1,packets[i].len,capture)!=packets[i].len)abort();
 }
}
static size_t build_input(const struct fixture *f,unsigned variant) {
 size_t off=0;
 if(variant==2)off=5; /* five leading_zero_8bits then three-byte prefixes */
 if(variant==3)off=7; /* seven leading_zero_8bits then four-byte prefixes */
 memset(input,0,sizeof(input));
 for(size_t n=0;n<f->count;n++) {
  size_t prefix=(variant==1 || variant==2 || (variant==4 && !(n&1)))?3:4;
  EXPECT(f->nals[n].length>=4);
  if(off+prefix+f->nals[n].length>sizeof(input))abort();
  input[off+prefix-1]=1;off+=prefix;
  memcpy(input+off,f->bytes+f->nals[n].offset,f->nals[n].length);
  off+=f->nals[n].length;
 }
 memcpy(before,input,off);return off;
}
/* Independent RTP/FU reconstruction oracle: no production NAL splitter or
 * packetization helper is used to compute expected payloads. */
static void verify_packets(const struct fixture *f,uint32_t timestamp,int raw) {
 size_t completed=0,used=0;int in_fu=0;
 for(size_t i=0;i<packet_count;i++) {
  const unsigned char *p=packets[i].data;size_t n=packets[i].len;
  EXPECT(n>=16 && n<=1472);if(n<16 || n>1472)continue;
  EXPECT(p[0]==0x80 && (p[1]&0x7f)==96);
  EXPECT(((unsigned)p[2]<<8|p[3])==(unsigned short)(65530+i));
  EXPECT(be32(p+4)==timestamp && be32(p+8)==0x12345678);
  p+=12;n-=12;
  int fu=f->hevc?((p[0]>>1)&63)==49:(p[0]&31)==28;
  if(fu) {
   size_t header=f->hevc?3:2;unsigned h=p[header-1];
   if(h&0x80) {
    EXPECT(!in_fu);in_fu=1;used=0;
    reconstructed[used++]=f->hevc?(p[0]&0x81)|((h&63)<<1):(p[0]&0xe0)|(h&31);
    if(f->hevc)reconstructed[used++]=p[1];
   }
   EXPECT(in_fu);if(used+n-header>sizeof(reconstructed))abort();
   memcpy(reconstructed+used,p+header,n-header);used+=n-header;
   if(!(h&0x40))continue;
   in_fu=0;
  } else {
   EXPECT(!in_fu);used=n;memcpy(reconstructed,p,n);
  }
  size_t expected=raw?f->count-1:completed;
  EXPECT(expected<f->count);
  if(expected<f->count) {
   EXPECT(used==f->nals[expected].length);
   if(used==f->nals[expected].length)
    EXPECT(memcmp(reconstructed,f->bytes+f->nals[expected].offset,used)==0);
  }
  completed++;used=0;
 }
 EXPECT(!in_fu);EXPECT(completed==(raw?1:f->count));
}
static void run_frame(const struct fixture *f,unsigned variant,uint64_t us,uint32_t ticks,int error) {
 char name[96];snprintf(name,sizeof(name),"%s-prefix%u%s",f->name,variant,error?"-send-error":"");case_name=name;case_id++;
 unsigned oldfail=failures;init();size_t size=build_input(f,variant);
 if(error)fail_call=5; /* fail after first fragment on the valid large fixture */
 hal_vidpack pack={.data=input,.length=(unsigned)size,.timestamp=us};
 hal_vidstream stream={.pack=&pack,.count=1,.seq=0xabcdef01};
 int status=rtp_send_h26x(&server,&stream,(char)f->hevc);
 EXPECT(status==(error?FAILURE:SUCCESS));
 EXPECT(memcmp(input,before,size)==0);
 if(error) { EXPECT(send_calls==5 && packet_count==4); }
 else verify_packets(f,ticks,0);
 record();printf("case=%u name=%s packets=%zu send_calls=%zu status=%d failures=%u\n",
  case_id,name,packet_count,send_calls,status,failures-oldfail);
 finish();
}
static void run_raw(const struct fixture *f) {
 case_name=f->name;case_id++;init();size_t n=f->count-1,size=f->nals[n].length;
 memcpy(input,f->bytes+f->nals[n].offset,size);memcpy(before,input,size);
 hal_vidpack pack={.data=input,.length=(unsigned)size,.timestamp=1234567};
 hal_vidstream stream={.pack=&pack,.count=1};
 EXPECT(rtp_send_h26x(&server,&stream,(char)f->hevc)==SUCCESS);
 EXPECT(memcmp(input,before,size)==0);verify_packets(f,111111,1);record();finish();
 printf("case=%u name=%s-raw packets=%zu\n",case_id,case_name,packet_count);
}
int main(int argc,char **argv) {
 case_name="setup";
 if(argc==3 && strcmp(argv[1],"--capture")==0) { capture=fopen(argv[2],"wbx");if(!capture)return 2; }
 else if(argc!=1)return 2;
 static const uint64_t us[]={1,1234567,48000000000ULL};
 static const uint32_t ticks[]={0,111111,25032704};
 for(size_t f=0;f<ARRAY_SIZE(fixtures);f++) {
  for(unsigned v=0;v<5;v++)run_frame(&fixtures[f],v,us[v%3],ticks[v%3],0);
  run_raw(&fixtures[f]);
  if(f&1)run_frame(&fixtures[f],0,1234567,111111,1);
 }
 if(capture && fclose(capture))return 2;
 printf("production RTP checks=%u failures=%u cases=%u\n",checks,failures,case_id);
 return failures?1:0;
}
