#include "../firmware/keyword_event.h"
#include <assert.h>
KeywordEvent score(float p, uint32_t seq, uint32_t ms, uint32_t generation=0) {
  return {p, ms*1000-60000, ms*1000, ms, seq, generation};
}
EventResult feed(KeywordEventFilter& f,float p,uint32_t seq,uint32_t ms,uint32_t gen=0) {
 return f.consume(score(p,seq,ms,gen),gen,ms*1000+8000,ms+8);
}
int main() {
 KeywordEventFilter f;
 assert(!feed(f,1,1,1000).detection.confirmed);
 assert(!feed(f,1,2,1128).detection.confirmed);
 assert(feed(f,1,3,1256).detection.confirmed);
 assert(!feed(f,1,4,1384).detection.confirmed);
 // Invalidation preserves a latched trigger/cooldown, avoiding repeated actions.
 assert(f.consume(score(1,5,1512),1,1520000,1520).discarded);
 assert(!feed(f,1,6,1640,1).detection.confirmed);
 assert(!feed(f,0,7,1768,1).detection.confirmed);
 assert(!feed(f,0,8,1896,1).detection.confirmed);
 assert(!feed(f,1,9,2024,1).detection.confirmed);
 assert(!feed(f,1,10,2152,1).detection.confirmed);
 assert(!feed(f,1,11,2280,1).detection.confirmed); // cooldown still active
 assert(feed(f,1,12,2800,1).detection.confirmed == false); // cadence gap resets evidence
 assert(!feed(f,1,13,2928,1).detection.confirmed);
 assert(feed(f,1,14,3056,1).detection.confirmed);
 KeywordEventFilter gap;
 feed(gap,1,1,1000);feed(gap,1,2,1128);
 assert(!feed(gap,1,4,1256).detection.confirmed); // missing queued score
 assert(!feed(gap,1,5,1384).detection.confirmed);
 assert(feed(gap,1,6,1512).detection.confirmed);
 KeywordEventFilter stale;
 assert(stale.consume(score(1,1,1000),0,1300001,1300).discarded);
 assert(stale.consume(score(1,2,1128),1,1130000,1130).discarded);
 for(int i=0;i<5;++i) assert(!feed(stale,1,1,1500+i*128).detection.confirmed); // duplicate sequence
 KeywordEventFilter latch;
 feed(latch,1,1,1000);feed(latch,1,2,1128);assert(feed(latch,1,3,1256).detection.confirmed);
 // A later gap plus moderate confidence must not manufacture a release.
 for(int i=0;i<10;++i) assert(!feed(latch,.7f,10+i,3000+i*128,1).detection.confirmed);
 for(int i=0;i<6;++i) assert(!feed(latch,1,20+i,4280+i*128,1).detection.confirmed);
 KeywordEventFilter wrap;
 KeywordEvent e{1,0xffff0000u,0xfffffff0u,1000,0xffffffffu,0};
 assert(!wrap.consume(e,0,16,1001).discarded);
 e.sequence=0;e.decisionUs=128000;e.decisionMs=1128;
 assert(!wrap.consume(e,0,128100,1128).detection.confirmed);
 e.sequence=1;e.decisionUs=256000;e.decisionMs=1256;
 assert(wrap.consume(e,0,256100,1256).detection.confirmed);
}
