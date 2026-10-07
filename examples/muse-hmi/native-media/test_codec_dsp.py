"""Check the real codec register encoding without requiring an attached board."""
import subprocess
from pathlib import Path


def test_codec_eq_gains_unity_off_and_reserved_fields(tmp_path):
 source=Path(__file__).with_suffix('.cpp')
 binary=tmp_path/'codec-test'
 subprocess.run(['g++','-std=c++17',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
