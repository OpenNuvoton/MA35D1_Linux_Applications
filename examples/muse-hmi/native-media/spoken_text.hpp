// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "generative_ui.hpp"
#include <sstream>
namespace muse_ui {
// Read what the Text panel actually renders, including values rather than JSON.
inline std::string spoken_text(const J& cached){
 if(!cached.is_object())return {};if(!cached.contains("ui"))return cached.value("text",std::string());const auto& doc=cached["ui"];validate(doc);std::ostringstream out;auto say=[&](const std::string& text){if(!text.empty()){out<<text;if(text.back()!='.'&&text.back()!='?'&&text.back()!='!')out<<'.';out<<' ';}};say(doc.value("title",std::string()));say(doc.value("subtitle",std::string()));
 for(const auto& b:doc["blocks"]){say(b.value("title",b.value("label",std::string())));auto type=b.value("type",std::string());if(type=="text")say(b.value("text",std::string()));else if(type=="metric"){say(display(b["value"])+" "+b.value("unit",std::string()));say(b.value("detail",std::string()));}else if(type=="metrics"){for(const auto& item:b["items"]){say(item.value("title",std::string())+": "+display(item["value"])+" "+item.value("unit",std::string()));say(item.value("detail",std::string()));}}else if(type=="progress")say(display(b["value"])+" of "+display(b.value("max",J(100)))+" "+b.value("unit",std::string()));else if(type=="table"){for(const auto& row:b["rows"]){std::string line;for(size_t i=0;i<row.size();i++)line+=b["columns"][i].get<std::string>()+": "+display(row[i])+", ";say(line);}}else{for(const auto& series:b["series"]){say(series.value("name",std::string()));for(size_t i=0;i<b["labels"].size();i++)say(b["labels"][i].get<std::string>()+": "+display(series["values"][i])+" "+b.value("unit",std::string()));}}}
 return out.str();
}
}
