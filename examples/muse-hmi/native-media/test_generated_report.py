"""Validate native PDF output with an independent PDF parser."""
import json
import subprocess
from pathlib import Path
import pytest

@pytest.fixture(scope='module')
def writer(tmp_path_factory):
 root=Path(__file__).parent
 path=tmp_path_factory.mktemp('pdf-writer');source=path/'main.cpp';binary=path/'writer'
 source.write_text('#include "generated_report.hpp"\n#include <iostream>\nint main(){muse_ui::J doc;std::cin>>doc;std::cout<<muse_ui::report_pdf(doc);}\n')
 subprocess.run(['g++','-std=c++17','-O0','-I',str(root),str(source),'-o',str(binary)],check=True)
 return binary

def test_pdf_has_actual_values_sources_and_pagination(writer):
 fitz=pytest.importorskip('fitz')
 doc={'title':'MLB (Athletics) \\ report','subtitle':'2026 season','sources':[{'title':'MLB','url':'https://statsapi.mlb.com/api/v1/teams/133/stats'}], 'blocks':[
 {'type':'metrics','items':[{'title':'Record','value':'64 - 98'},{'title':'HR','value':194}]},
 {'type':'bar','title':'Runs','labels':['Scored','Allowed'],'series':[{'name':'Runs','values':[699,937]}]},
 {'type':'table','columns':['Player','Value'],'rows':[[f'Player {i}',i] for i in range(100)]},
 {'type':'text','text':'Final paragraph after the table.'}]}
 data=subprocess.check_output([str(writer)],input=json.dumps(doc).encode())
 pdf=fitz.open(stream=data,filetype='pdf')
 assert len(pdf)>=4
 text=''.join(page.get_text() for page in pdf)
 for value in ['64 - 98','194','699','937','Player 99','Final paragraph','https://statsapi.mlb.com/api/v1/teams/133/stats']:
  assert value in text
 for page in pdf:
  pix=page.get_pixmap(matrix=fitz.Matrix(.25,.25))
  assert pix.width==153 and pix.height==198
