/*  file: ttyART.c
    created on: 7 Oct 2023

    compilation:
      first provide olive.c file,
      ref: https://github.com/tsoding/olive.c.git
      then:
          cc -o ttyART ttyART.c -lm

      run it on tty (display server must be detached)  
 **/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

#define OLIVEC_IMPLEMENTATION
#include "olive.c"

#define RENDER vc_render1
Olivec_Canvas vc_render1(float dt, int WIDTH, int HEIGHT, Olivec_Canvas oc);


int main() {
    struct fb_var_screeninfo vinfo;
    int fb_fd;

    fb_fd = open("/dev/fb0", O_RDWR);

    if (fb_fd == -1) {
        perror("Error opening framebuffer device");
        return 1;
    }

    if (ioctl (fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable information");
        return 2;
    }

    int screensize = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;

    int* fbp = (int*)mmap(
                 0,
                 screensize,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 fb_fd,
             0);

    if ((intptr_t)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        return 3;
    }

    printf("ssize=%d, yv=%d, xv=%d, bpp=%d\n", screensize, 
           vinfo.yres_virtual, vinfo.xres_virtual, vinfo.bits_per_pixel);
    printf("offsets -- x=%d, y=%d\n", vinfo.xoffset, vinfo.yoffset);


    Olivec_Canvas oc = olivec_canvas(fbp, vinfo.xres, vinfo.yres, vinfo.xres);
    oc = olivec_subcanvas(oc, 1190, 550, 800, 500);

    for(;;){
            RENDER (0.05f, 800, 500, oc);
            usleep(1000*1000/10);
    }


    munmap(fbp, screensize);
    close(fb_fd);

    return 0;
}


float sqrtf(float x);
float atan2f(float y, float x);
float sinf(float x);
float cosf(float x);

#define PI 3.14159265359

#define BACKGROUND_COLOR 0xFF000000
#define GRID_COUNT 10
#define GRID_PAD 0.5/GRID_COUNT
#define GRID_SIZE ((GRID_COUNT - 1)*GRID_PAD)
#define CIRCLE_RADIUS 5
#define Z_START 0.25
#define ABOBA_PADDING 50

static float angle = 0;

Olivec_Canvas vc_render1(float dt, int WIDTH, int HEIGHT, Olivec_Canvas oc)
{
    angle += 0.25*PI*dt;

    olivec_fill(oc, BACKGROUND_COLOR);
    for (int ix = 0; ix < GRID_COUNT; ++ix) {
        for (int iy = 0; iy < GRID_COUNT; ++iy) {
            for (int iz = 0; iz < GRID_COUNT; ++iz) {
                float x = ix*GRID_PAD - GRID_SIZE/2;
                float y = iy*GRID_PAD - GRID_SIZE/2;
                float z = Z_START + iz*GRID_PAD;

                float cx = 0.0;
                float cz = Z_START + GRID_SIZE/2;

                float dx = x - cx;
                float dz = z - cz;

                float a = atan2f(dz, dx);
                float m = sqrtf(dx*dx + dz*dz);

                dx = cosf(a + angle)*m;
                dz = sinf(a + angle)*m;

                x = dx + cx;
                z = dz + cz;

                x /= z;
                y /= z;

                uint32_t r = ix*255/GRID_COUNT;
                uint32_t g = iy*255/GRID_COUNT;
                uint32_t b = iz*255/GRID_COUNT;
                uint32_t color = 0xFF000000 | (r<<(0*8)) | (g<<(1*8)) | (b<<(2*8));
                olivec_circle(oc, (x + 1)/2*WIDTH, (y + 1)/2*HEIGHT, CIRCLE_RADIUS, color);
            }
        }
    }

    size_t size = 8;

    return oc;
}

