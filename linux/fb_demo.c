//
//  main.cpp
//  frame buffer GIF demo
//
//  Created by Laurence Bank on 12/2/21.
//  Copyright © 2021 Laurence Bank. All rights reserved.
//
#include "../src/AnimatedGIF.h"
#include "../src/gif.inl"

#include "../test_images/badgers.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>

GIFIMAGE gif;
int iGIFWidth, iGIFHeight;
uint8_t *pGIFBuf;

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
uint8_t *fbp; // start of frame buffer pointer
int iPitch; // bytes per line on the display
int iScreenWidth, iScreenHeight;
volatile int iStop = 0;

// CTRL-C handler
void my_handler(int signal)
{
   iStop = 1;
} /* my_handler() */

//
// Display a GIF frame on the framebuffer
//
void ShowFrame(int bFullscreen)
{
    // Check for 32-bit or 16-bit display
    if (vinfo.bits_per_pixel == 32) {
        uint32_t ulPixel = 0, *pul;
        uint8_t *s, *pPal;
        int pixel, x, y;

        if (bFullscreen) { // stretch to fit
            int iFracX, iFracY; // x/y stretch fractions
            int newpixel, iAccumX, iAccumY;
            iFracX = (iGIFWidth * 256) / iScreenWidth;
            iFracY = (iGIFHeight * 256) / iScreenHeight;
            iAccumY = 0;
            for (y = 0; y < iScreenHeight; y++) {
                pul = (uint32_t *)&fbp[iPitch * y];
                s = &pGIFBuf[(iAccumY >> 8) * iGIFWidth];
                iAccumY += iFracY;
                iAccumX = 0;
                pixel = -1;
                for (x = 0; x < iScreenWidth; x++) {
                    newpixel = s[(iAccumX >> 8)];
                    if (newpixel != pixel) {
                        pixel = newpixel;
                        pPal = (uint8_t *)&gif.pPalette;
                        pPal += pixel * 3;
                        ulPixel = (pPal[0] << 16) | (pPal[1] << 8) | pPal[2];
                    }
                    *pul++ = ulPixel;
                    iAccumX += iFracX;
                }
            } // for y
        } else { // draw 1:1
            for (y = 0; y < iGIFHeight; y++) {
                pul = (uint32_t *)&fbp[iPitch * y];
                s = &pGIFBuf[y * iGIFWidth];
                for (x = 0; x < iGIFWidth; x++) {
                    pixel = *s++;
                    pPal = (uint8_t *)&gif.pPalette;
                    pPal += pixel * 3;
                    ulPixel = (pPal[0] << 16) | (pPal[1] << 8) | pPal[2];
                    *pul++ = ulPixel;
                }
            } // for y
        } // 1:1
    } else if (vinfo.bits_per_pixel == 16) {
        uint16_t usPixel = 0, *pus;
        uint8_t *s, *pPal, r, g, b;
        int pixel, x, y;

        if (bFullscreen) { // stretch to fit
            int iFracX, iFracY; // x/y stretch fractions
            int newpixel, iAccumX, iAccumY;
            iFracX = (iGIFWidth * 256) / iScreenWidth;
            iFracY = (iGIFHeight * 256) / iScreenHeight;
            iAccumY = 0;
            for (y = 0; y < iScreenHeight; y++) {
                pus = (uint16_t *)&fbp[iPitch * y];
                s = &pGIFBuf[(iAccumY >> 8) * iGIFWidth];
                iAccumY += iFracY;
                iAccumX = 0;
                pixel = -1;
                for (x = 0; x < iScreenWidth; x++) {
                    newpixel = s[(iAccumX >> 8)];
                    if (newpixel != pixel) {
                        pixel = newpixel;
                        pPal = (uint8_t *)&gif.pPalette;
                        pPal += pixel * 3;
                        r = pPal[0] >> 3; // 8-bit to 5-bit
                        g = pPal[1] >> 2; // 8-bit to 6-bit
                        b = pPal[2] >> 3; // 8-bit to 5-bit
                        usPixel = (r << 11) | (g << 5) | b;
                    }
                    *pus++ = usPixel;
                    iAccumX += iFracX;
                }
            } // for y
        } else { // draw 1:1
            for (y = 0; y < iGIFHeight; y++) {
                pus = (uint16_t *)&fbp[iPitch * y];
                s = &pGIFBuf[y * iGIFWidth];
                for (x = 0; x < iGIFWidth; x++) {
                    pixel = *s++;
                    pPal = (uint8_t *)&gif.pPalette;
                    pPal += pixel * 3;
                    r = pPal[0] >> 3; // 8-bit to 5-bit
                    g = pPal[1] >> 2; // 8-bit to 6-bit
                    b = pPal[2] >> 3; // 8-bit to 5-bit
                    usPixel = (r << 11) | (g << 5) | b;
                    *pus++ = usPixel;
                }
            } // for y
        } // 1:1
    }
} /* ShowFrame() */

//
// Callback from GIF library for each line decoded
//
void GIFDraw(GIFDRAW *pDraw)
{
uint8_t *s, *d;
int x, y;

    y = pDraw->iY + pDraw->y; // current line
    s = pDraw->pPixels;
    d = &pGIFBuf[pDraw->iX + (y * iGIFWidth)];
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<pDraw->iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t c, ucTransparent = pDraw->ucTransparent;
      for (x=0; x < pDraw->iWidth; x++)
      {
          c = *s++;
          if (c != ucTransparent)
          {
            d[x] = c; 
          }
      }
    }
    else
    {
      s = pDraw->pPixels;
      memcpy(d, s, pDraw->iWidth);
    }
} /* GIFDraw() */
void print_usage(void)
{
    printf("Usage: fb_demo [--loop N] [filename]\n");
    printf("Run with no parameters to test in-memory decoding\n");
    printf("Or pass a filename on the command line\n");
    printf("Optional parameter: --loop N, loop N iterations (default: infinite)\n");
}
int main(int argc, const char * argv[]) {
    int screensize, fbfd = 0, rc;
    struct sigaction sigIntHandler;
    char *szInFile = NULL;
    int iLoopCount = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--loop") == 0) {
            if (i + 1 < argc) {
                iLoopCount = atoi(argv[++i]);
            } else {
                printf("Error: --loop requires a number.\n");
                print_usage();
                return -1;
            }
        } else if (argv[i][0] == '-') {
            printf("Error: Unknown option '%s'\n", argv[i]);
            print_usage();
            return -1;
        } else {
            if (szInFile) {
                printf("Error: Multiple filenames specified: '%s' and '%s'\n", szInFile, argv[i]);
                print_usage();
                return -1;
            }
            szInFile = (char *)argv[i];
        }
    }

    // Set CTRL-C signal handler
    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    printf("Animated GIF Linux Demo\n");
    if (iLoopCount >= 0)
        printf("Loop count: %d\n", iLoopCount);
    else
        printf("Loop count: infinite\n");
    printf("Run with no parameters to test in-memory decoding\n");
    printf("Or pass a filename on the command line\n\n");

    // Access the framebuffer
    fbfd = open("/dev/fb1", O_RDWR);
    if (!fbfd) {
        printf("Error opening framebuffer device; try disabling the VC4 overlay\n");
        return -1;
    }
    // Get the fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
       printf("Error reading the framebuffer fixed information.\n");
       close(fbfd);
       return -1;
    }
      // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
       printf("Error reading the framebuffer variable information.\n");
       close(fbfd);
       return -1;
    }
    printf("%dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );
    // Map framebuffer to user memory
    iScreenWidth = vinfo.xres;
    iScreenHeight = vinfo.yres;
    iPitch = (iScreenWidth * vinfo.bits_per_pixel)/8;
    screensize = finfo.smem_len;
    fbp = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    // FIX 2: Correct way to check for mmap failure
    if (fbp == MAP_FAILED) {
       printf("Failed to mmap the framebuffer.\n");
       close(fbfd);
       return -1;
    }

    GIF_begin(&gif, BIG_ENDIAN_PIXELS);
    printf("Starting GIF decoder...\n");
    if (szInFile)
        rc = GIF_openFile(&gif, szInFile, GIFDraw);
    else
        rc = GIF_openRAM(&gif, (uint8_t *)badgers, sizeof(badgers), GIFDraw);
    if (rc)
    {
        printf("Successfully opened GIF\n");
        iGIFWidth = GIF_getCanvasWidth(&gif);
        iGIFHeight = GIF_getCanvasHeight(&gif);
        printf("Image size: %d x %d\n", iGIFWidth, iGIFHeight);
        gif.ucDrawType = GIF_DRAW_RAW; // we want the original 8-bit pixels
        gif.ucPaletteType = GIF_PALETTE_RGB888;
        // Allocate a buffer to hold the current GIF frame
        pGIFBuf = malloc(iGIFWidth * iGIFHeight);
        if (!pGIFBuf) {
            printf("Failed to allocate memory for GIF buffer\n");
            GIF_close(&gif);
            munmap(fbp, screensize);
            close(fbfd);
            return -1;
        }
        {
            int iDelay;
            if (iLoopCount < 0) {
                while (!iStop) {
                    while (GIF_playFrame(&gif, &iDelay, NULL)) {
                        if (iStop) break;
                        ShowFrame(1);
                        usleep(iDelay * 1000);
                    }
                    GIF_reset(&gif);
                }
            } else {
                for (int i = 0; i < iLoopCount && !iStop; i++) {
                    while (GIF_playFrame(&gif, &iDelay, NULL)) {
                        if (iStop) break;
                        ShowFrame(1);
                        usleep(iDelay * 1000);
                    }
                    GIF_reset(&gif);
                }
            }
        }
        GIF_close(&gif);
    }
    // Cleanup
    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}
