// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "vendor/json.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
namespace muse_ui {
using J=nlohmann::json;
inline void check(bool value,const char* error){if(!value)throw std::runtime_error(error);}
inline void label(const J& o,const char* key,size_t limit=160){if(o.contains(key))check(o[key].is_string()&&o[key].get<std::string>().size()<=limit,"UI label too long or invalid");}
inline void number(const J& v){check(v.is_number()&&std::isfinite(v.get<double>())&&std::abs(v.get<double>())<=1e12,"UI value must be a finite number within +/-1e12");}
inline void validate(const J& doc){
 check(doc.is_object(),"UI document must be an object");label(doc,"title");label(doc,"subtitle",512);label(doc,"source",512);label(doc,"theme",32);if(doc.contains("team_id"))check(doc["team_id"].is_number_integer()&&doc["team_id"].get<int>()>0&&doc["team_id"].get<int>()<1000,"invalid team ID");if(doc.contains("sources")){check(doc["sources"].is_array()&&doc["sources"].size()<=24,"too many sources");for(const auto& source:doc["sources"]){check(source.is_object(),"invalid source");label(source,"title",160);label(source,"url",1024);}}
 check(doc.contains("blocks")&&doc["blocks"].is_array()&&doc["blocks"].size()<=24,"UI requires at most 24 blocks");
 for(const auto& b:doc["blocks"]){check(b.is_object(),"UI block must be an object");label(b,"title");label(b,"label");label(b,"unit",24);label(b,"id",64);auto type=b.value("type",std::string());
 if(type=="text"){label(b,"text",4096);}
 else if(type=="metrics"){check(b.contains("items")&&b["items"].is_array()&&!b["items"].empty()&&b["items"].size()<=6,"metrics requires 1..6 items");for(const auto& item:b["items"]){check(item.is_object()&&item.contains("value"),"invalid metric item");label(item,"title",80);label(item,"unit",24);if(item["value"].is_number())number(item["value"]);else label(item,"value",80);}}
 else if(type=="metric"){check(b.contains("value")&&(b["value"].is_string()||b["value"].is_number()),"metric value required");if(b["value"].is_number())number(b["value"]);else label(b,"value",80);label(b,"detail",160);}
 else if(type=="progress"){check(b.contains("value"),"progress value required");number(b["value"]);double max=b.value("max",100.0);check(std::isfinite(max)&&max>0&&max<=1e12,"invalid progress maximum");check(b["value"].get<double>()>=0&&b["value"].get<double>()<=max,"progress outside range");}
 else if(type=="bar"||type=="line"){
 check(b.contains("labels")&&b["labels"].is_array()&&!b["labels"].empty()&&b["labels"].size()<=48,"chart requires 1..48 labels");for(const auto& l:b["labels"])check(l.is_string()&&l.get<std::string>().size()<=80,"invalid chart label");
 check(b.contains("series")&&b["series"].is_array()&&!b["series"].empty()&&b["series"].size()<=6,"chart requires 1..6 series");for(const auto& series:b["series"]){check(series.is_object(),"invalid chart series");label(series,"name",80);check(series.contains("values")&&series["values"].is_array()&&series["values"].size()==b["labels"].size(),"chart values must match labels");for(const auto& v:series["values"])number(v);}
 }
 else if(type=="table"){
 check(b.contains("columns")&&b["columns"].is_array()&&!b["columns"].empty()&&b["columns"].size()<=8,"table requires 1..8 columns");for(const auto& c:b["columns"])check(c.is_string()&&c.get<std::string>().size()<=80,"invalid table column");
 check(b.contains("rows")&&b["rows"].is_array()&&b["rows"].size()<=100,"table requires at most 100 rows");for(const auto& row:b["rows"]){check(row.is_array()&&row.size()==b["columns"].size(),"table row width mismatch");for(const auto& cell:row){check(cell.is_string()||cell.is_number(),"table cells must be text or numbers");if(cell.is_string())check(cell.get<std::string>().size()<=160,"table cell too long");else number(cell);}}
 }
 else throw std::runtime_error("unsupported UI block: "+type);
 }
 check(doc.dump().size()<=180000,"UI document too large");
}
inline void slides_validate(const J& slides){check(slides.is_array()&&slides.size()<=20,"slideshow allows at most 20 photos");for(const auto& slide:slides){check(slide.is_object(),"invalid slide");label(slide,"url",4096);label(slide,"caption",180);label(slide,"source",1024);check(slide.contains("url")&&slide["url"].get<std::string>().rfind("https://",0)==0,"slide requires a direct HTTPS image URL");}}
inline std::string topic_source(const J& doc){for(const auto& source:doc.value("sources",J::array())){auto url=source.value("url",std::string());if(url.rfind("https://en.wikipedia.org/wiki/",0)==0){auto end=url.find_first_of("?#");if(end!=std::string::npos)url.resize(end);return url;}}return {};}
inline bool same_topic(const J& old,const J& doc){if(!old.is_object())return false;if(old.value("title",std::string())==doc.value("title",std::string()))return true;auto source=topic_source(old);return !source.empty()&&source==topic_source(doc);}
inline void presentation_validate(const J& p){check(p.is_object(),"presentation must be an object");check(p.value("version",std::string("1"))=="1","unsupported GenUI version");validate(p.at("document"));if(p.contains("pending_media"))check(p["pending_media"].is_boolean(),"pending_media must be boolean");if(p.contains("slides"))slides_validate(p["slides"]);if(p.contains("video")){check(p["video"].is_object(),"invalid video");label(p["video"],"url",4096);label(p["video"],"caption",180);auto k=p["video"].value("kind",std::string("video"));check(k=="video"||k=="youtube","video kind must be video or youtube");check(p["video"].value("url",std::string()).rfind("https://",0)==0,"video requires HTTPS");}if(p.contains("interval_seconds"))check(p["interval_seconds"].is_number_integer()&&p["interval_seconds"].get<int>()>=1&&p["interval_seconds"].get<int>()<=60,"slide interval must be 1..60 seconds");}
inline int header_height(const J&){return 84;}
inline J from_text(const std::string& text){auto start=text.find("```muse-ui");if(start==std::string::npos)return nullptr;start=text.find('\n',start);if(start==std::string::npos)return nullptr;auto end=text.find("```",start);if(end==std::string::npos)return nullptr;try{auto doc=J::parse(text.substr(start,end-start));validate(doc);return doc;}catch(...){return nullptr;}}
inline int block_height(const J& b){auto type=b.value("type",std::string());return type=="bar"||type=="line"?248:type=="metrics"?94:type=="table"?70+int(b["rows"].size())*30:type=="metric"?98:type=="progress"?88:60+int(b.value("text",std::string()).size()/55)*25;}
inline std::string display(const J& v){return v.is_string()?v.get<std::string>():v.dump();}
}
