"""Persistent SSH framebuffer transport with dirty rectangular updates."""
import os
from pathlib import Path
import select
import struct
import subprocess
import time

WIDTH, HEIGHT, STRIDE = 1024, 600, 4096

def frame_packet(raw, previous, scroll=0):
    if len(raw)!=STRIDE*HEIGHT:raise ValueError('invalid frame length')
    rectangles=[]
    if previous is not None and 0<abs(scroll)<380:
        # Rotated chat viewport: move existing pixels on the board.
        x,y,w,h=350,96,650,380
        rectangles.append(struct.pack('<4Ii',x,y,w|0x80000000,h,scroll))
        shifted=bytearray(previous)
        rows=range(scroll,h) if scroll>0 else range(0,h+scroll)
        for j in rows:
            dst=(y+j)*STRIDE+x*4;src=(y+j-scroll)*STRIDE+x*4
            shifted[dst:dst+w*4]=previous[src:src+w*4]
        previous=bytes(shifted)
    # Compare tiles; merge adjacent changed tiles horizontally.
    for y in range(0,HEIGHT,32):
        h=min(32,HEIGHT-y);start=None
        for x in range(0,WIDTH+64,64):
            changed=x<WIDTH and (previous is None or any(
                raw[r*STRIDE+x*4:r*STRIDE+(x+64)*4]!=previous[r*STRIDE+x*4:r*STRIDE+(x+64)*4]
                for r in range(y,y+h)))
            if changed and start is None:start=x
            if not changed and start is not None:
                w=x-start
                data=b''.join(raw[r*STRIDE+start*4:r*STRIDE+x*4] for r in range(y,y+h))
                rectangles.append(struct.pack('<4I',start,y,w,h)+data)
                start=None
    return struct.pack('<I',len(rectangles))+b''.join(rectangles)

class FramebufferWriter:
    def __init__(self,ssh):
        self.ssh=ssh;self.proc=None;self.previous=None;self.suspended=False;self.last_probe=0

    def connect(self):
        self.close()
        helper=Path(__file__).with_name('framebuffer_writer').read_bytes()
        subprocess.run(self.ssh+['cat > /tmp/muse-framebuffer-writer.new && chmod 755 /tmp/muse-framebuffer-writer.new && mv /tmp/muse-framebuffer-writer.new /tmp/muse-framebuffer-writer'],input=helper,capture_output=True,timeout=10,check=True)
        self.proc=subprocess.Popen(self.ssh+['/tmp/muse-framebuffer-writer'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,bufsize=0)
        os.set_blocking(self.proc.stdin.fileno(),False)

    def write(self,raw,scroll=0):
        if self.proc is None or self.proc.poll() is not None:self.connect()
        if self.suspended and time.monotonic()-self.last_probe<1:return False
        packet=bytes(4) if self.suspended else frame_packet(raw,self.previous,scroll)
        if packet[:4]==bytes(4) and not self.suspended:return False
        deadline=time.monotonic()+3
        try:
            view=memoryview(packet)
            while view:
                remaining=deadline-time.monotonic()
                if remaining<=0 or not select.select([], [self.proc.stdin], [], remaining)[1]:raise TimeoutError('framebuffer transport stalled')
                try:n=os.write(self.proc.stdin.fileno(),view)
                except BlockingIOError:continue
                view=view[n:]
            remaining=deadline-time.monotonic()
            if remaining<=0 or not select.select([self.proc.stdout],[],[],remaining)[0]:raise TimeoutError('framebuffer acknowledgement stalled')
            ack=self.proc.stdout.read(1)
            if ack==b'N':
                self.suspended=True;self.last_probe=time.monotonic();self.previous=None
                return False
            if ack!=b'K':raise RuntimeError('framebuffer writer disconnected')
            if self.suspended:
                self.suspended=False;self.previous=None
                return False
            self.previous=raw
            return True
        except Exception:
            self.close();raise

    def close(self):
        if self.proc is not None:
            self.proc.terminate()
            try:self.proc.wait(timeout=1)
            except subprocess.TimeoutExpired:self.proc.kill();self.proc.wait()
            self.proc.stdin.close();self.proc.stdout.close();self.proc=None
        self.previous=None
