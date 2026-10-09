// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "vendor/json.hpp"
#include <chrono>
#include <cstring>
#include <cstdio>
namespace muse_ui {
struct Progress {
 int generation=0,visible=0,revision=0;int tabs[4]{};double hide_at=0,started=0;char topic[160]{},detail[160]{},history[3][96]{},errors[4][160]{};
 static double clock(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 void activity(const std::string& line){if(line.empty())return;snprintf(detail,sizeof(detail),"%s",line.c_str());if(line!=history[2]){memcpy(history[0],history[1],96);memcpy(history[1],history[2],96);snprintf(history[2],96,"%s",line.c_str());}revision++;}
 void begin(const std::string& title){generation++;visible=1;hide_at=0;started=clock();for(int& tab:tabs)tab=0;snprintf(topic,sizeof(topic),"%s",title.c_str());memset(history,0,sizeof(history));memset(errors,0,sizeof(errors));activity("Sending request to Muse");}
 int percent()const{int done=0,total=0;for(int tab:tabs)if(tab!=4){total++;if(tab==2)done++;}return total?done*100/total:100;}
 nlohmann::json status()const{nlohmann::json list=nlohmann::json::array(),events=nlohmann::json::array();const char* names[]={"text","image","pdf","video"};const char* states[]={"waiting","loading","ready","error","unavailable"};for(int i=0;i<4;i++)list.push_back({{"kind",names[i]},{"state",states[tabs[i]]},{"error",errors[i]}});for(const auto& line:history)if(line[0])events.push_back(line);return {{"generation",generation},{"visible",bool(visible)},{"percent",percent()},{"elapsed_seconds",started>0?int(clock()-started):0},{"topic",topic},{"detail",detail},{"activity",events},{"tabs",list}};}
};
}
