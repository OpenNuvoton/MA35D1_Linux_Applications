"""Frames remain distinguishable after switching to a new native media worker."""
import struct,subprocess
from pathlib import Path

def test_overlay_frame_sequence_survives_worker_replacement(tmp_path):
 root=Path(__file__).parent
 source=tmp_path/'writer.cpp';binary=tmp_path/'writer';frame=tmp_path/'frame'
 source.write_text('#include "overlay_frame.hpp"\n#include <vector>\n#include <cstdlib>\nint main(int argc,char**argv){std::vector<unsigned> pixels(1024*600,std::strtoul(argv[2],nullptr,16));publish_overlay_frame(pixels.data(),pixels.size()*4,argv[1]);}\n')
 subprocess.run(['g++','-std=c++17','-I',str(root),str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary),str(frame),'ff112233'],check=True)
 first=frame.read_bytes();header=struct.unpack('<IIII',first[:16]);assert header==(0x55494652,2,1024,600)
 subprocess.run([str(binary),str(frame),'ff445566'],check=True)
 second=frame.read_bytes();assert struct.unpack('<I',second[4:8])[0]==4
 assert struct.unpack('<I',second[16:20])[0]==0xff445566
 assert len(second)==16+1024*600*4
