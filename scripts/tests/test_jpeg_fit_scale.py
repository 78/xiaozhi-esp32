"""Exercise the real JPEG preview fit_scale() on the host."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "main/display/lvgl_display/jpg/jpeg_to_image.c"


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


@unittest.skipUnless(shutil.which("cc"), "requires a host C compiler")
class FitScaleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.binary = str(pathlib.Path(cls.tmp.name) / "fit-scale-test")
        driver = r'''
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

typedef struct {
    int width;
    int height;
} jpeg_resolution_t;

''' + function(SOURCE.read_text(), "static void fit_scale(") + r'''

int main(int argc, char** argv) {
    jpeg_resolution_t scale;
    fit_scale(atoi(argv[1]), atoi(argv[2]), strtoul(argv[3], NULL, 10),
              strtoul(argv[4], NULL, 10), &scale);
    printf("%d %d\n", scale.width, scale.height);
    return 0;
}
'''
        source = pathlib.Path(cls.tmp.name) / "fit_scale_test.c"
        source.write_text(driver)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", str(source), "-o", cls.binary],
                       check=True)

    def fit(self, src_width, src_height, max_width, max_height):
        result = subprocess.run(
            [self.binary, str(src_width), str(src_height), str(max_width), str(max_height)],
            check=True, capture_output=True, text=True)
        return tuple(int(value) for value in result.stdout.split())

    def assert_decoder_accepts(self, src_width, src_height, scaled):
        width, height = scaled
        self.assertEqual((width % 8, height % 8), (0, 0))
        self.assertGreaterEqual(width * 8, src_width)
        self.assertGreaterEqual(height * 8, src_height)

    def test_rounding_at_one_eighth_bumps_to_minimum_aligned_size(self):
        # 1920x1080 into 240x240 is exactly 1/8; 135 rounds down to 128, below 1080 / 8
        scaled = self.fit(1920, 1080, 240, 240)
        self.assertEqual(scaled, (240, 136))
        self.assert_decoder_accepts(1920, 1080, scaled)

    def test_scales_down_to_fit_bounds(self):
        scaled = self.fit(640, 480, 320, 240)
        self.assertEqual(scaled, (320, 240))
        self.assert_decoder_accepts(640, 480, scaled)

    def test_leaves_scaling_disabled_when_image_fits(self):
        self.assertEqual(self.fit(320, 240, 320, 240), (0, 0))
        self.assertEqual(self.fit(320, 240, 0, 0), (0, 0))

    def test_falls_back_to_minimum_size_beyond_one_eighth(self):
        # A 128x32 screen needs ~1/22 for a 720p frame; decode at the 1/8 minimum, not full size
        scaled = self.fit(1280, 720, 128, 32)
        self.assertEqual(scaled, (160, 96))
        self.assert_decoder_accepts(1280, 720, scaled)
        scaled = self.fit(1920, 1080, 120, 120)
        self.assertEqual(scaled, (240, 136))
        self.assert_decoder_accepts(1920, 1080, scaled)

    def test_small_source_fits_tiny_screen(self):
        scaled = self.fit(320, 240, 128, 32)
        self.assertEqual(scaled, (40, 32))
        self.assert_decoder_accepts(320, 240, scaled)

    def test_never_upscales_a_thin_side(self):
        # The minimum aligned height (8) is taller than the 4-pixel source
        self.assertEqual(self.fit(1920, 4, 240, 240), (0, 0))

    def test_single_bound_limits_one_side(self):
        scaled = self.fit(1920, 1080, 480, 0)
        self.assertEqual(scaled, (480, 270 // 8 * 8))
        self.assert_decoder_accepts(1920, 1080, scaled)


if __name__ == "__main__":
    unittest.main()
