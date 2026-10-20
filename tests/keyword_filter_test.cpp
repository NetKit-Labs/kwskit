#include "../firmware/keyword_filter.h"
#include <assert.h>
int main() {
 KeywordFilter f;
 assert(!f.update(1,0)); assert(!f.update(0,128));
 assert(!f.update(1,256)); assert(!f.update(1,384));
 assert(f.update(1,512));
 for(int i=1;i<20;++i) assert(!f.update(1,512+i*128));
 assert(!f.update(0,3100)); assert(!f.update(0,3228));
 assert(!f.update(1,3356)); assert(!f.update(1,3484)); assert(f.update(1,3612));
 KeywordFilter g;
 assert(!g.update(1,0)); assert(!g.update(1,128)); assert(g.update(1,256));
 for(int i=0;i<4;++i) assert(!g.update(0,384+i*128));
 for(int i=0;i<6;++i) assert(!g.update(1,896+i*128));
 assert(g.update(1,1800));
}
