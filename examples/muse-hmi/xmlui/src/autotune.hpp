// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "model.hpp"
#include <array>
#include <functional>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <QDir>
#include <QSaveFile>
class AutoTune {
 std::function<J(const std::string&,const J&)> media;
 QString folder;
 std::array<int,5> winner{},candidate{},original{};
 std::array<int,5> direction{{-1,-1,-1,-1,-1}};
 bool active=false,originalHardware=false,originalSoftware=true,originalPlaying=false;
 J originalAudio=nullptr;
 int trial=0;
 static constexpr int frequencies[5]={105,300,850,2400,6900};
 void profile(const std::array<int,5>& values){
  for(int i=0;i<5;i++){auto r=media("media.control",{{"operation","hardware_equalizer"},{"band",i},{"value",values[i]}});if(!r.value("ok",false))throw std::runtime_error("Codec EQ adjustment failed");}
  media("media.control",{{"operation","dsp"},{"feature","software_eq"},{"enabled",false}});
  media("media.control",{{"operation","dsp"},{"feature","hardware_eq"},{"enabled",true}});
 }
 void next(){candidate=winner;int band=trial%5,step=trial<5?3:direction[band];candidate[band]=std::clamp(winner[band]+step,-6,6);}
 QString tone(double frequency,double seconds=1.5,double level=-25){if(!std::isfinite(frequency)||frequency<80||frequency>8000||!std::isfinite(seconds)||seconds<.2||seconds>3||!std::isfinite(level)||level<-36||level>-18)throw std::runtime_error("Tone requires 80..8000 Hz, 0.2..3 seconds, -36..-18 dBFS");QString path=folder+"/assets/calibration-"+QString::number(int(frequency))+".wav";QDir().mkpath(folder+"/assets");std::ofstream file(path.toStdString(),std::ios::binary);if(!file)throw std::runtime_error("Cannot create calibration tone");auto write16=[&](uint16_t x){file.put(x&255);file.put(x>>8);};auto write32=[&](uint32_t x){write16(x&65535);write16(x>>16);};constexpr int rate=48000;int count=std::lround(seconds*rate);file.write("RIFF",4);write32(36+count*4);file.write("WAVEfmt ",8);write32(16);write16(1);write16(2);write32(rate);write32(rate*4);write16(4);write16(16);file.write("data",4);write32(count*4);for(int i=0;i<count;i++){double envelope=std::min(1.0,std::min(i/960.0,(count-1-i)/960.0));int16_t value=std::lround(32767*std::pow(10.,level/20.)*envelope*std::sin(6.283185307179586*frequency*i/rate));write16(value);write16(value);}return path;}
void restoreAudio(){media("media.control",{{"operation","stop"}});media("media.cache_restore",{{"kind","audio"},{"params",originalAudio}});if(originalPlaying&&!originalAudio.is_null())media("media.show",originalAudio);}
public:
 J generatedTone(const J& p){auto frequency=p.value("frequency_hz",440.0),seconds=p.value("duration_seconds",1.5),level=p.value("level_dbfs",-25.0);auto path=tone(frequency,seconds,level);auto out=media("media.show",{{"kind","audio"},{"url",path.toStdString()},{"caption","Generated listening tone"}});if(!out.value("ok",false))throw std::runtime_error("Tone playback failed");return {{"frequency_hz",frequency},{"duration_seconds",seconds},{"level_dbfs",level},{"generated_on_board",true}};}
 AutoTune(std::function<J(const std::string&,const J&)> callback,QString path):media(callback),folder(path){}
 J status()const{return {{"active",active},{"trial",trial},{"comparisons",10},{"frequency_hz",frequencies[trial%5]},{"profile_a",winner},{"profile_b",candidate},{"tone_level_dbfs",-25},{"tone_seconds",1.5},{"question",active?"Listen to A and B at the same listening volume. Which sounds better: A, B, or the same?":"Ask whether the user wants guided home-theater tuning."},{"method","User preference A/B on the stereo codec; no microphone measurement"}};}
 // Internal renderer-maintenance snapshot; never a listener preference or playback action.
 void suspend(){active=false;}
 J checkpoint()const{return {{"active",active},{"trial",trial},{"winner",winner},{"candidate",candidate},{"direction",direction},{"original",original},{"original_hardware",originalHardware},{"original_software",originalSoftware},{"original_playing",originalPlaying},{"original_audio",originalAudio}};}
 void resume(const J& p){
  if(active)throw std::runtime_error("Calibration already active");
  auto readProfile=[&](const char* key,int limit){auto a=p.at(key).get<std::array<int,5>>();for(int v:a)if(v < -limit||v > limit)throw std::runtime_error("Invalid calibration snapshot gains");return a;};
  auto w=readProfile("winner",6),c=readProfile("candidate",6),o=readProfile("original",12),d=readProfile("direction",1);for(int v:d)if(v!=1&&v!=-1)throw std::runtime_error("Invalid calibration direction");
  int t=p.at("trial").get<int>();if(t<0||t>=10||!p.at("active").get<bool>())throw std::runtime_error("Invalid calibration snapshot round");
  auto expected=w;expected[t%5]=std::clamp(w[t%5]+(t<5?3:d[t%5]),-6,6);if(c!=expected)throw std::runtime_error("Invalid calibration comparison");
  bool h=p.at("original_hardware").get<bool>(),s=p.at("original_software").get<bool>(),playing=p.at("original_playing").get<bool>();auto audio=p.at("original_audio");if(!audio.is_null()&&(!audio.is_object()||audio.value("kind",std::string())!="audio"))throw std::runtime_error("Invalid original audio");
  winner=w;candidate=c;original=o;direction=d;trial=t;originalHardware=h;originalSoftware=s;originalPlaying=playing;originalAudio=audio;active=true;
 }
 J run(const J& p){auto op=p.value("operation",std::string("status"));if(op=="status")return status();if(op=="start"){if(active)throw std::runtime_error("Calibration already active");auto r=media("media.status",{});if(!r.value("ok",false))throw std::runtime_error("Media service unavailable");auto s=r.at("payload");if(!s.value("hardware_eq_enabled",false)&&!s.at("dsp").value("available",false))throw std::runtime_error("NAU8822 codec unavailable");original=s.at("hardware_eq_db").get<std::array<int,5>>();originalHardware=s.value("hardware_eq_enabled",false);originalSoftware=s.value("software_eq_enabled",true);originalPlaying=s.value("audio_running",false);originalAudio=nullptr;try{QFile f("/mnt/sd/muse/media-cache/tabs.json");if(f.open(QIODevice::ReadOnly)){auto cache=J::parse(f.readAll().constData());if(cache.is_array()&&cache.size()==5)originalAudio=cache[4];}}catch(...){}winner=originalHardware?original:std::array<int,5>{};for(auto& v:winner)v=std::clamp(v,-6,6);direction={{-1,-1,-1,-1,-1}};trial=0;active=true;next();return status();}if(!active)throw std::runtime_error("Start calibration first");if(op=="a"||op=="b"){profile(op=="a"?winner:candidate);auto path=tone(frequencies[trial%5]);auto r=media("media.show",{{"kind","audio"},{"url",path.toStdString()},{"caption","Calibration tone "+std::to_string(frequencies[trial%5])+" Hz"}});if(!r.value("ok",false))throw std::runtime_error("Calibration playback failed");return status();}if(op=="choose"){auto choice=p.value("choice",std::string());if(choice!="a"&&choice!="b"&&choice!="same")throw std::runtime_error("Choose a, b or same");if(choice=="b"){winner=candidate;direction[trial%5]=1;}profile(winner);trial++;if(trial>=10){active=false;restoreAudio();QSaveFile file(folder+"/preferred-eq.json");if(file.open(QIODevice::WriteOnly)){file.write(J({{"five_band_db",winner},{"method","listener preference"}}).dump().c_str());file.commit();}auto out=status();out["finished"]=true;out["question"]="The preferred EQ is saved. Does the user like it with their normal music?";return out;}next();return status();}if(op=="cancel"){profile(original);media("media.control",{{"operation","dsp"},{"feature","software_eq"},{"enabled",originalSoftware}});media("media.control",{{"operation","dsp"},{"feature","hardware_eq"},{"enabled",originalHardware}});restoreAudio();active=false;return status();}throw std::runtime_error("Unknown calibration operation");}
};
