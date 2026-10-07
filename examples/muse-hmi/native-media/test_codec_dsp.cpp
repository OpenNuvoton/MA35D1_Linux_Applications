// SPDX-License-Identifier: Apache-2.0
#include "codec_dsp.hpp"
#include <cassert>
int main(){
 unsigned char registers[]={0,0x2c,0,0xac,0,0x4c,0,0x6c,0,0x2c};
 auto check=[&](bool enabled){
  std::array<int,5> gains={12,-12,6,-6,0};
  muse_audio::Codec::encode(registers,gains,enabled);
  assert(registers[0]==1); // Must remain in playback path.
  for(int i=0;i<5;i++)assert((registers[i*2+1]&31)==12-(enabled?gains[i]:0));
  assert((registers[3]&~31)==0xa0); // Preserve bandwidth and frequency.
  assert((registers[5]&~31)==0x40);
  assert((registers[7]&~31)==0x60);
 };
 check(true);check(false);check(true);
}
