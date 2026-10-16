// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <array>
#include <string>
#include "vendor/json.hpp"

inline int media_category(const std::string& k){
 if(k=="text")return 0;if(k=="image"||k=="svg")return 1;
 if(k=="pdf")return 2;if(k=="video"||k=="youtube"||k=="animation")return 3;
 if(k=="audio")return 4;return -1;
}
inline const char* media_category_name(int i){static const char* names[]={"text","image","pdf","video","audio"};return i>=0&&i<5?names[i]:"unknown";}
struct MediaTabs {
 using J=nlohmann::json;
 std::array<J,5> last;
 int active=-1;
 void remember(int i,const J& params){last.at(i)=params;active=i;}
 J status() const {J out=J::array();for(int i=0;i<5;i++)out.push_back({{"kind",media_category_name(i)},{"cached",!last[i].is_null()},{"active",i==active}});return out;}
};
