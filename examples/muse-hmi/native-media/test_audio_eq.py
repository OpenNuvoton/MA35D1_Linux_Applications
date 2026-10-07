"""Verify audible EQ transfer gains, stereo isolation, and actual FFT measurements."""
import json
import subprocess
from pathlib import Path
import pytest

@pytest.fixture(scope='module')
def dsp(tmp_path_factory):
 root=Path(__file__).parent
 binary=tmp_path_factory.mktemp('eq-dsp')/'test-eq'
 subprocess.run(['g++','-std=c++17','-O2',str(root/'test_eq_dsp.cpp'),'-o',str(binary)],check=True)
 return binary

@pytest.mark.parametrize('band,frequency',list(enumerate([31.5,63,125,250,500,1000,2000,4000,8000,16000])))
@pytest.mark.parametrize('gain',[-6,6])
def test_real_filter_gain_at_each_center(dsp,band,frequency,gain):
 result=subprocess.check_output([str(dsp),str(band),str(gain),str(frequency)],text=True).split()
 assert float(result[0])==pytest.approx(gain,abs=.3)
 assert result[2]=='1', 'Processing the left channel must not leak audio to the right'

def test_flat_eq_preserves_pcm_exactly(dsp):
 result=subprocess.check_output([str(dsp),'5','0','1000'],text=True).split()
 assert result[1]=='1'
 assert int(result[3])==5
 assert float(result[9])>0

def test_spectrum_silence_is_zero(dsp):
 result=subprocess.check_output([str(dsp),'5','0','0'],text=True).split()
 assert all(float(level)==0 for level in result[4:])
