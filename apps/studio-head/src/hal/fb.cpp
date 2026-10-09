#include "hal/fb.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

uint8_t* fbp = nullptr;
fb_var_screeninfo vinfo = {};
fb_fix_screeninfo finfo = {};
bool bgr = false;  // 32 bpp con el rojo en el byte bajo (XBGR8888)

}  // namespace

bool fb_open(const char* path) {
    const int fd = open(path, O_RDWR);
    if (fd < 0) {
        std::perror("studio-head: no se pudo abrir el framebuffer");
        return false;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        std::perror("studio-head: ioctl del framebuffer");
        close(fd);
        return false;
    }
    if (vinfo.bits_per_pixel != 16 && vinfo.bits_per_pixel != 32) {
        std::fprintf(stderr, "studio-head: framebuffer de %u bpp no soportado (solo 16 o 32)\n",
                     vinfo.bits_per_pixel);
        close(fd);
        return false;
    }
    const size_t size = size_t(finfo.line_length) * vinfo.yres_virtual;
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);  // el mapeo sigue vivo sin el descriptor
    if (p == MAP_FAILED) {
        std::perror("studio-head: mmap del framebuffer");
        return false;
    }
    fbp = static_cast<uint8_t*>(p);
    bgr = vinfo.bits_per_pixel == 32 && vinfo.red.offset == 0;
    std::printf("studio-head: framebuffer %ux%u, %u bpp%s, línea de %u bytes\n", vinfo.xres, vinfo.yres,
                vinfo.bits_per_pixel, bgr ? " (BGR)" : "", finfo.line_length);
    return true;
}

void fb_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    if (fbp) {
        const int w = lv_area_get_width(area);
        const int x1 = std::max<int>(area->x1, 0);
        const int x2 = std::min<int>(area->x2, int(vinfo.xres) - 1);
        const int y1 = std::max<int>(area->y1, 0);
        const int y2 = std::min<int>(area->y2, int(vinfo.yres) - 1);
        const int bytes_pp = int(vinfo.bits_per_pixel / 8);

        for (int y = y1; y <= y2 && x1 <= x2; ++y) {
            uint8_t* row = fbp + size_t(y + vinfo.yoffset) * finfo.line_length + size_t(x1 + vinfo.xoffset) * bytes_pp;
            const lv_color_t* src = color_p + size_t(y - area->y1) * w + (x1 - area->x1);
            const int n = x2 - x1 + 1;
#if LV_COLOR_DEPTH == 16
            if (vinfo.bits_per_pixel == 16) {
                std::memcpy(row, src, size_t(n) * 2);
                continue;
            }
#endif
            uint32_t* dst = reinterpret_cast<uint32_t*>(row);
            for (int i = 0; i < n; ++i) {
                uint32_t c = lv_color_to32(src[i]);  // 0xAARRGGBB
                if (bgr) c = (c & 0xFF00FF00u) | ((c >> 16) & 0xFFu) | ((c & 0xFFu) << 16);
                dst[i] = c;
            }
        }
    }
    lv_disp_flush_ready(drv);
}
