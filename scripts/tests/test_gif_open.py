"""Exercise real GIF loading and cleanup with host LVGL allocation/filesystem adapters."""

import os
import shlex
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class GifOpenTest(unittest.TestCase):
    def test_memory_file_and_allocation_failure_cleanup(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("requires a host C compiler")
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "esp_log.h").write_text("#define ESP_LOGW(...) ((void)0)\n")
            (build / "lvgl.h").write_text(r'''
                #include <stdbool.h>
                #include <stddef.h>
                #include <stdint.h>
                #include <limits.h>
                #include <stdio.h>
                #define LV_USE_DRAW_SW_ASM 0
                #define LV_DRAW_SW_ASM_HELIUM 1
                #define LV_FS_MODE_RD 0
                #define LV_FS_RES_OK 0
                #define LV_FS_SEEK_SET SEEK_SET
                #define LV_FS_SEEK_CUR SEEK_CUR
                typedef int lv_fs_res_t;
                typedef struct { FILE* file; } lv_fs_file_t;
                void* lv_malloc(size_t);
                void* lv_realloc(void*, size_t);
                void lv_free(void*);
                int lv_fs_open(lv_fs_file_t*, const char*, int);
                int lv_fs_close(lv_fs_file_t*);
                int lv_fs_read(lv_fs_file_t*, void*, uint32_t, uint32_t*);
                int lv_fs_seek(lv_fs_file_t*, uint32_t, int);
                int lv_fs_tell(lv_fs_file_t*, uint32_t*);
            ''')
            (build / "driver.c").write_text(r'''
                #include "gifdec.h"
                #include <assert.h>
                #include <stdlib.h>
                #include <string.h>
                static int outstanding, opened, calls, fail_at;
                void* lv_malloc(size_t size) {
                    if (++calls == fail_at) return NULL;
                    void* p = malloc(size);
                    if (p) ++outstanding;
                    return p;
                }
                void* lv_realloc(void* p, size_t size) {
                    if (++calls == fail_at) return NULL;
                    bool was_null = p == NULL;
                    void* result = realloc(p, size);
                    if (result && was_null) ++outstanding;
                    return result;
                }
                void lv_free(void* p) { if (p) { --outstanding; free(p); } }
                int lv_fs_open(lv_fs_file_t* fd, const char* name, int mode) {
                    (void)mode;
                    fd->file = fopen(name, "rb");
                    if (fd->file) ++opened;
                    return fd->file ? 0 : -1;
                }
                int lv_fs_close(lv_fs_file_t* fd) { --opened; return fclose(fd->file); }
                int lv_fs_read(lv_fs_file_t* fd, void* dst, uint32_t n, uint32_t* read) {
                    size_t size = fread(dst, 1, n, fd->file);
                    if (read) *read = size;
                    return size == n ? 0 : -1;
                }
                int lv_fs_seek(lv_fs_file_t* fd, uint32_t pos, int mode) {
                    return fseek(fd->file, pos, mode);
                }
                int lv_fs_tell(lv_fs_file_t* fd, uint32_t* pos) {
                    *pos = ftell(fd->file); return 0;
                }
                static const unsigned char image[] = {
                    'G','I','F','8','9','a', 1,0,1,0,0x80,0,0,
                    0,0,0,255,255,255, ',',0,0,0,0,1,0,1,0,0, 2,2,0x4c,1,0,';'
                };
                static void reset(int fail) {
                    assert(outstanding == 0 && opened == 0);
                    calls = 0; fail_at = fail;
                }
                static gd_GIF* load(int file, const char* path, const void* data) {
                    return file ? gd_open_gif_file(path) : gd_open_gif_data(data);
                }
                int main(int argc, char** argv) {
                    assert(argc == 2);
                    FILE* output = fopen(argv[1], "wb"); assert(output);
                    assert(fwrite(image, 1, sizeof(image), output) == sizeof(image));
                    fclose(output);
                    for (int file = 0; file < 2; ++file) {
                        reset(1);
                        assert(load(file, argv[1], image) == NULL);
                        assert(outstanding == 0 && opened == 0);
                        reset(0);
                        gd_GIF* gif = load(file, argv[1], image); assert(gif);
                        assert(calls == 1);
                        assert(outstanding == 1 && opened == file);
                        assert(gif->width == 1 && gif->height == 1);
                        assert(gd_get_frame(gif) == 1);
                        unsigned char pixel[4] = {0};
                        gd_render_frame(gif, pixel);
                        for (int i = 0; i < 4; ++i) assert(pixel[i] == 255);
                        gd_close_gif(gif);
                        assert(outstanding == 0 && opened == 0);
                    }
                    unsigned char invalid[sizeof(image)]; memcpy(invalid, image, sizeof(image));
                    invalid[0] = 'B';
                    reset(0); assert(gd_open_gif_data(invalid) == NULL);
                    output = fopen(argv[1], "wb"); assert(output);
                    fwrite(invalid, 1, sizeof(invalid), output); fclose(output);
                    reset(0); assert(gd_open_gif_file(argv[1]) == NULL);
                    output = fopen(argv[1], "wb"); assert(output);
                    fwrite(image, 1, 6, output); fclose(output);
                    reset(0); assert(gd_open_gif_file(argv[1]) == NULL);
                    reset(0); assert(gd_open_gif_file("missing-gif") == NULL);
                    reset(0);
                }
            ''')
            source = ROOT / "main/display/lvgl_display/gif"
            for cache in (0, 1):
                with self.subTest(cache=cache):
                    executable = build / f"gif-open-{cache}"
                    result = subprocess.run(
                        compiler + ["-std=c11", f"-DLV_GIF_CACHE_DECODE_DATA={cache}",
                                    f"-I{build}", f"-I{source}", str(build / "driver.c"),
                                    str(source / "gifdec.c"), "-o", str(executable)],
                        capture_output=True, text=True,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    result = subprocess.run([str(executable), str(build / "image.gif")],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
