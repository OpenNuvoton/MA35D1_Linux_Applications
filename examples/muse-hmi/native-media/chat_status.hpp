// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "vendor/json.hpp"
#include <string>
namespace muse_chat {
using J=nlohmann::json;
struct Status {
 J value={{"request",nullptr},{"task",nullptr},{"approvals",nullptr}};
 void submitted(long long id,const std::string& text){value["request"]={{"stream_id",id},{"message",text},{"state","submitted"}};value["task"]=nullptr;value["approvals"]=nullptr;}
 void response(long long id,int code,const std::string& body,bool ended){if(value["request"].is_object()&&value["request"].value("stream_id",0LL)==id){auto& request=value["request"];request["http_status"]=code;request["state"]=code>=400?"rejected":ended?"acknowledged":"receiving";if(ended){try{request["response"]=J::parse(body);}catch(...){request["response"]=body.substr(0,2000);}}}}
 bool event(const J& fields){std::string name=fields.value("event_name",fields.value("event",std::string()));if(name=="task.status"||name=="agent.status"){J task={{"event",name}};for(const auto* key:{"status","activity_text","task_id","session_id"})if(fields.contains(key))task[key]=fields[key];value[name=="task.status"?"task":"agent"]=task;return true;}if(name=="approvals.snapshot"){value["approvals"]=fields.value("pending_approvals",J::array());return true;}if(name=="message.assistant"||name=="delta.message_done"){for(const auto* key:{"display_text","content","text"})if(fields.contains(key)&&fields[key].is_string()){value["last_assistant_text"]=fields[key].get<std::string>().substr(0,4000);return true;}}return false;}
};
}
