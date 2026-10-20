#include "../firmware/fft_power.h"
#include <assert.h>
int main() {
  float packed[512] = {}, power[257];
  packed[0]=512; fftPower(packed,power,512);
  assert(power[0]==512*512 && power[256]==0);
  packed[0]=0; packed[1]=512; fftPower(packed,power,512);
  assert(power[0]==0 && power[256]==512*512);
  packed[1]=0; packed[14]=3; packed[15]=4;
  fftPower(packed,power,512); assert(power[7]==25);
  for(int i=0;i<257;++i) if(i!=7) assert(power[i]==0);
}
