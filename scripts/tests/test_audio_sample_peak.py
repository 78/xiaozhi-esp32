"""PCM peak must handle silence and the asymmetric signed-16-bit limits."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AudioSamplePeakTests(unittest.TestCase):
    def test_pcm_bounds_without_buffer_copy(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        driver = r'''
#include <cassert>
#include "audio/sample_peak.h"
int main() {
    assert(AudioSamplePeak(nullptr,0)==0);
    int16_t silence[]={0,0,0};
    assert(AudioSamplePeak(silence,3)==0);
    int16_t values[]={-4,12,-200,35};
    assert(AudioSamplePeak(values,4)==200);
    assert(values[2]==-200);
    int16_t extremes[]={32767,-32768,0};
    assert(AudioSamplePeak(extremes,1)==32767);
    assert(AudioSamplePeak(extremes,3)==32768);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source, binary = path / "test.cc", path / "test.exe"
            source.write_text(driver, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I",
                            str(ROOT / "main"), str(source), "-o", str(binary)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
