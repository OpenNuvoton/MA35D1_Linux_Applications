// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "quick_sources.hpp"
namespace muse_ui {
struct PresentationPolicy {
 bool owned=false,published=false;
 std::string topic,token;
 static std::string key(const std::string& text){auto s=muse_quick::article(muse_quick::topic(text));std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return std::tolower(c);});return s;}
 bool begin(const std::string& text,const std::string& id,bool explicit_request=false){auto next=key(text);if(!explicit_request&&owned&&next==topic)return false;topic=next;token=id;owned=true;published=false;return true;}
 bool retain(const std::string& command,const nlohmann::json& params=nlohmann::json::object())const{if(command=="media.show"&&params.value("kind",std::string())=="audio")return false;return owned&&(command=="genui.present"||command=="media.present"||command=="genui.fetch"||command=="genui.prepare"||command=="media.ui"||command=="media.show"||command=="display.picture"||command=="display.image"||command=="sports.baseball");}
 nlohmann::json state()const{return {{"owned",owned},{"published",published},{"topic",topic},{"token",token}};}
 void restore(const nlohmann::json& value){published=value.value("published",false);owned=published&&value.value("owned",false);topic=value.value("topic",std::string());token=value.value("token",std::string());}
};
}
