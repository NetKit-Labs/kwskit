#include "../firmware/keyword_detector.h"
#include <assert.h>
int main() {
  KeywordDetector detector;
  assert(!detector.update(1,1000,2000,1).confirmed);
  assert(!detector.update(1,129000,130000,129).confirmed);
  auto result = detector.update(1,257000,258000,257);
  assert(result.confirmed && result.hasResponse && result.responseUs==257000);
  assert(!detector.update(1,385000,386000,385).confirmed);
  detector.reset();
  assert(!detector.update(1,0xfffffff0u,10,1000).confirmed);
  assert(!detector.update(1,100000,100100,1128).confirmed);
  result=detector.update(1,200000,200100,1256);
  assert(result.confirmed && result.hasResponse && result.responseUs==200116);
}
