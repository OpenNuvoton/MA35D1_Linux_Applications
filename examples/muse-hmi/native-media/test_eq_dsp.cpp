// SPDX-License-Identifier: Apache-2.0
#include "audio_eq.hpp"
#include <iostream>
#include <vector>
int main(int argc,char**argv){
 if(argc!=4)return 2;int band=std::stoi(argv[1]);double gain=std::stod(argv[2]),frequency=std::stod(argv[3]);
 std::vector<int16_t> pcm(48000*4);for(size_t i=0;i<pcm.size()/2;i++)pcm[i*2]=int16_t(std::round(1000*std::sin(2*muse_audio::pi*frequency*i/48000)));
 auto original=pcm;muse_audio::Equalizer eq;std::array<double,10> gains{};gains.at(band)=gain;eq.configure(gains,false);eq.process(pcm.data(),pcm.size()/2);
 double before=0,after=0;bool right_silent=true;for(size_t i=48000;i<pcm.size()/2;i++){before+=double(original[2*i])*original[2*i];after+=double(pcm[2*i])*pcm[2*i];right_silent&=pcm[2*i+1]==0;}
 muse_audio::Spectrum spectrum;spectrum.process(pcm.data(),pcm.size()/2);auto levels=spectrum.levels();int dominant=std::max_element(levels.begin(),levels.end())-levels.begin();
 std::cout<<20*std::log10(after>0?std::sqrt(after/before):1)<<" "<<(pcm==original)<<" "<<right_silent<<" "<<dominant;for(double level:levels)std::cout<<" "<<level;std::cout<<"\n";
}
