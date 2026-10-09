import socket
import threading
import json
from muse_genui import Client, Executor, COMMAND_SPECS, device_description, unix_run

def test_sdk_registration_and_executor():
 calls=[]
 def board(command,params,timeout):
  calls.append((command,params,timeout));return {'ok':True}
 class Base:
  def run(self,*args):return {'base':args[0]}
 executor=Executor(Base(),board)
 client=Client(executor.run)
 doc={'title':'Meta news','blocks':[{'type':'text','text':'Sourced news summary'}]}
 assert client.present(doc)['ok']
 assert calls[0][0]=='genui.present' and calls[0][1]['document']==doc
 assert client.fetch('Beijing')['ok']
 assert calls[-1]==('genui.fetch',{'topic':'Beijing'},3000)
 assert executor.run('device.health',{})=={'base':'device.health'}
 description=device_description(node_id='test',display_name='Board',commands={'device.health':{}})
 assert set(COMMAND_SPECS)<=description.register_params()['commands_v2'].keys()

def test_unix_transport_uses_board_protocol(tmp_path):
 path=str(tmp_path/'board.sock');server=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
 server.bind(path);server.listen(1);received=[]
 def serve():
  with server.accept()[0] as client:
   received.append(json.loads(client.recv(65536)))
   client.sendall(b'{"ok":true}\n')
 thread=threading.Thread(target=serve);thread.start()
 try:
  assert unix_run('genui.status',{},path=path)['ok']
  thread.join(2)
  assert received==[{'action':'genui.status','params':{}}]
 finally:server.close()
