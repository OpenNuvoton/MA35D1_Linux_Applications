"""Keep gateway submission distinct from Muse acceptance and pending approval."""
import json
import subprocess
from pathlib import Path
import pytest

@pytest.fixture(scope='module')
def status_binary(tmp_path_factory):
 root=Path(__file__).parent;path=tmp_path_factory.mktemp('chat-status')
 source=path/'main.cpp';binary=path/'test'
 source.write_text('''#include "chat_status.hpp"
#include <iostream>
int main(){muse_chat::J data;std::cin>>data;muse_chat::Status s;s.submitted(3,"Taiwan");for(const auto& event:data["events"])s.event(event);if(data.contains("response"))s.response(data.value("id",3),data["code"],data["response"].dump(),true);std::cout<<s.value;}
''')
 subprocess.run(['g++','-std=c++17','-I',str(root),str(source),'-o',str(binary)],check=True)
 return binary

def run(binary,**params):
 return json.loads(subprocess.check_output([str(binary)],input=json.dumps({'events':[],**params}).encode()))

def test_submission_is_not_acceptance(status_binary):
 assert run(status_binary)['request']['state']=='submitted'
 accepted=run(status_binary,code=200,response={'accepted':True})
 assert accepted['request']['state']=='acknowledged'
 assert accepted['request']['response']=={'accepted':True}
 assert run(status_binary,id=4,code=200,response={})['request']['state']=='submitted'

def test_rejection_and_approval_stay_visible(status_binary):
 result=run(status_binary,code=403,response={'error':'not allowed'},events=[
 {'event_name':'task.status','status':'pending_user_confirmation'},
 {'event_name':'approvals.snapshot','pending_approvals':[{'command':'genui.present'}]}])
 assert result['request']['state']=='rejected'
 assert result['task']['status']=='pending_user_confirmation'
 assert result['approvals'][0]['command']=='genui.present'
