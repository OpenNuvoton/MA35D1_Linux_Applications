// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>
#include <algorithm>
#include <cctype>
namespace muse_media {
// Prefer display-sized Commons photos instead of downloading multi-megabyte originals.
inline std::string image_source(std::string url){const std::string root="https://upload.wikimedia.org/wikipedia/commons/";if(url.rfind(root,0)!=0)return url;auto path=url.substr(root.size());path=path.substr(0,path.find_first_of("?#"));if(path.rfind("thumb/",0)==0)path=path.substr(6);auto first=path.find('/'),second=path.find('/',first==std::string::npos?first:first+1);if(first==std::string::npos||second==std::string::npos)return url;auto third=path.find('/',second+1);path=path.substr(0,third);auto file=path.substr(second+1),lower=file;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return std::tolower(c);});auto dot=lower.rfind('.');if(dot==std::string::npos)return url;auto ext=lower.substr(dot);if(ext!=".jpg"&&ext!=".jpeg"&&ext!=".png"&&ext!=".webp")return url;return root+"thumb/"+path+"/960px-"+file;}
}
