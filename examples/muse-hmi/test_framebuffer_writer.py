import shutil
import struct
import subprocess
from pathlib import Path
import pytest
from framebuffer_writer import frame_packet,STRIDE,HEIGHT

@pytest.fixture
def native(tmp_path):
    if not shutil.which('gcc'):pytest.skip('host C compiler unavailable')
    binary=tmp_path/'writer'
    subprocess.run(['gcc','-O2',str(Path(__file__).with_name('framebuffer_writer.c')),'-o',str(binary)],check=True)
    return binary

def test_native_transport_writes_full_frame_then_sparse_rectangles(native,tmp_path):
    output=tmp_path/'fb';output.write_bytes(bytes(STRIDE*HEIGHT))
    initial=bytes([12])*STRIDE*HEIGHT
    changed=bytearray(initial)
    offset=123*STRIDE+401*4;changed[offset:offset+4]=b'BGRA'
    sparse=frame_packet(bytes(changed),initial)
    assert len(sparse)<10000
    p=subprocess.run([str(native),str(output)],input=frame_packet(initial,None)+sparse,capture_output=True)
    assert p.returncode==0 and p.stdout==b'KK'
    assert output.read_bytes()==bytes(changed)

def test_identical_frame_has_no_updates():
    raw=bytes(STRIDE*HEIGHT)
    assert frame_packet(raw,raw)==bytes(4)

def test_native_rejects_out_of_bounds_write(native,tmp_path):
    output=tmp_path/'fb';output.write_bytes(bytes(STRIDE*HEIGHT))
    p=subprocess.run([str(native),str(output)],input=struct.pack('<5I',1,1023,0,2,1),capture_output=True)
    assert p.returncode==4 and not p.stdout

def test_incomplete_frame_is_not_acknowledged(native,tmp_path):
    output=tmp_path/'fb';output.write_bytes(bytes(STRIDE*HEIGHT))
    p=subprocess.run([str(native),str(output)],input=struct.pack('<5I',1,0,0,64,32)+b'x',capture_output=True)
    assert p.returncode==5 and not p.stdout


@pytest.mark.parametrize('shift',[3,-3])
def test_native_scroll_matches_full_frame_without_transferring_existing_pixels(native,tmp_path,shift):
    output=tmp_path/'fb';initial=bytearray(STRIDE*HEIGHT)
    # Unique rows to prove overlap direction preserves original pixels.
    for y in range(96,476):initial[y*STRIDE+350*4:y*STRIDE+1000*4]=bytes([y%255])*2600
    output.write_bytes(initial);changed=bytearray(initial)
    rows=range(shift,380) if shift>0 else range(0,380+shift)
    for j in rows:
        dst=(96+j)*STRIDE+350*4;src=(96+j-shift)*STRIDE+350*4
        changed[dst:dst+2600]=initial[src:src+2600]
    packet=frame_packet(bytes(changed),bytes(initial),scroll=shift)
    assert len(packet)<50000
    p=subprocess.run([str(native),str(output)],input=packet,capture_output=True)
    assert p.returncode==0 and p.stdout==b'K'
    assert output.read_bytes()==bytes(changed)
