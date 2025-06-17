//
//  main.c
//  frame buffer GIF demo
//
//  Created by Laurence Bank on 12/2/21.
//  Copyright © 2021 Laurence Bank. All rights reserved.
//
//  Modificado para aceptar los parámetros --in y --loop
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
uint8_t *fbp; // Puntero al inicio del framebuffer
int iPitch; // Bytes por línea en la pantalla
int iScreenWidth, iScreenHeight;
volatile int iStop = 0;

// Manejador para CTRL-C
void my_handler(int signal)
{
   iStop = 1;
} /* my_handler() */

//
// Muestra un cuadro de GIF en el framebuffer
//
void ShowFrame(int bFullscreen)
{
    // Comprueba si la pantalla es de 32 o 16 bits
    if (vinfo.bits_per_pixel == 32) {
        uint32_t ulPixel = 0, *pul;
        uint8_t *s, *pPal;
        int pixel, x, y;

        if (bFullscreen) { // Estirar para ajustar
            int iFracX, iFracY; // Fracciones de estiramiento x/y
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
        } else { // Dibujar 1:1
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

        if (bFullscreen) { // Estirar para ajustar
            int iFracX, iFracY; // Fracciones de estiramiento x/y
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
                        r = pPal[0] >> 3; // 8-bit a 5-bit
                        g = pPal[1] >> 2; // 8-bit a 6-bit
                        b = pPal[2] >> 3; // 8-bit a 5-bit
                        usPixel = (r << 11) | (g << 5) | b;
                    }
                    *pus++ = usPixel;
                    iAccumX += iFracX;
                }
            } // for y
        } else { // Dibujar 1:1
            for (y = 0; y < iGIFHeight; y++) {
                pus = (uint16_t *)&fbp[iPitch * y];
                s = &pGIFBuf[y * iGIFWidth];
                for (x = 0; x < iGIFWidth; x++) {
                    pixel = *s++;
                    pPal = (uint8_t *)&gif.pPalette;
                    pPal += pixel * 3;
                    r = pPal[0] >> 3; // 8-bit a 5-bit
                    g = pPal[1] >> 2; // 8-bit a 6-bit
                    b = pPal[2] >> 3; // 8-bit a 5-bit
                    usPixel = (r << 11) | (g << 5) | b;
                    *pus++ = usPixel;
                }
            } // for y
        } // 1:1
    }
} /* ShowFrame() */

//
// Callback de la biblioteca GIF por cada línea decodificada
//
void GIFDraw(GIFDRAW *pDraw)
{
uint8_t *s, *d;
int x, y;

    y = pDraw->iY + pDraw->y; // Línea actual
    s = pDraw->pPixels;
    d = &pGIFBuf[pDraw->iX + (y * iGIFWidth)];
    if (pDraw->ucDisposalMethod == 2) // Restaurar al color de fondo
    {
      for (x=0; x<pDraw->iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }
    // Aplicar los nuevos píxeles a la imagen principal
    if (pDraw->ucHasTransparency) // Si se usa transparencia
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

// --- MODIFICADO ---
// Muestra las instrucciones de uso del programa
void print_usage(void)
{
    printf("Usage: fb_demo [--in <FILE>] [--loop N]\n");
    printf("Ejecutar sin parámetros para probar la decodificación en memoria.\n");
    printf("Opciones:\n");
    printf("  --in <FILE>   Ruta al archivo GIF a mostrar.\n");
    printf("  --loop N      Repetir N iteraciones (por defecto: infinito).\n");
}

int main(int argc, const char * argv[]) {
    int screensize, fbfd = 0, rc;
    struct sigaction sigIntHandler;
    char *szInFile = NULL;
    int iLoopCount = -1; // -1 para bucle infinito por defecto

    // --- MODIFICADO ---
    // Analiza los argumentos de la línea de comandos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--loop") == 0) {
            if (i + 1 < argc) {
                iLoopCount = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: --loop requiere un número.\n");
                print_usage();
                return -1;
            }
        } else if (strcmp(argv[i], "--in") == 0) {
            if (i + 1 < argc) {
                if (szInFile) {
                    fprintf(stderr, "Error: la opción --in se especificó más de una vez.\n");
                    print_usage();
                    return -1;
                }
                szInFile = (char *)argv[++i];
            } else {
                fprintf(stderr, "Error: --in requiere un nombre de archivo.\n");
                print_usage();
                return -1;
            }
        } else {
            fprintf(stderr, "Error: Opción desconocida '%s'\n", argv[i]);
            print_usage();
            return -1;
        }
    }

    // Establece el manejador de la señal CTRL-C
    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    printf("Demostración de GIF animado para Framebuffer de Linux\n");
    if (szInFile) {
        printf("Archivo de entrada: %s\n", szInFile);
    } else {
        printf("Usando imagen interna en memoria.\n");
    }
    if (iLoopCount >= 0) {
        printf("Repitiendo %d veces.\n", iLoopCount);
    } else {
        printf("Repitiendo indefinidamente (presiona CTRL-C para parar).\n");
    }
    printf("\n");

    // Accede al framebuffer
    fbfd = open("/dev/fb1", O_RDWR);
    if (!fbfd) {
        fbfd = open("/dev/fb0", O_RDWR); // Intenta con fb0 si fb1 falla
    }
    if (!fbfd) {
        fprintf(stderr, "Error al abrir el dispositivo framebuffer; intenta deshabilitar el overlay VC4\n");
        return -1;
    }
    // Obtiene la información fija de la pantalla
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
       fprintf(stderr, "Error al leer la información fija del framebuffer.\n");
       close(fbfd);
       return -1;
    }
      // Obtiene la información variable de la pantalla
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
       fprintf(stderr, "Error al leer la información variable del framebuffer.\n");
       close(fbfd);
       return -1;
    }
    printf("Resolución: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );
    // Mapea el framebuffer a la memoria del usuario
    iScreenWidth = vinfo.xres;
    iScreenHeight = vinfo.yres;
    iPitch = finfo.line_length;
    screensize = finfo.smem_len;
    fbp = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (fbp == MAP_FAILED) {
       fprintf(stderr, "Fallo al mapear el framebuffer.\n");
       close(fbfd);
       return -1;
    }

    GIF_begin(&gif, BIG_ENDIAN_PIXELS);
    printf("Iniciando decodificador GIF...\n");
    if (szInFile)
        rc = GIF_openFile(&gif, szInFile, GIFDraw);
    else
        rc = GIF_openRAM(&gif, (uint8_t *)badgers, sizeof(badgers), GIFDraw);
    
    if (rc)
    {
        printf("GIF abierto exitosamente\n");
        iGIFWidth = GIF_getCanvasWidth(&gif);
        iGIFHeight = GIF_getCanvasHeight(&gif);
        printf("Tamaño de la imagen: %d x %d\n", iGIFWidth, iGIFHeight);
        gif.ucDrawType = GIF_DRAW_RAW; // Queremos los píxeles originales de 8 bits
        gif.ucPaletteType = GIF_PALETTE_RGB888;
        // Asigna un búfer para contener el cuadro GIF actual
        pGIFBuf = malloc(iGIFWidth * iGIFHeight);
        if (!pGIFBuf) {
            fprintf(stderr, "Fallo al asignar memoria para el búfer del GIF\n");
            GIF_close(&gif);
            munmap(fbp, screensize);
            close(fbfd);
            return -1;
        }
        
        int iDelay;
        if (iLoopCount < 0) { // Bucle infinito
            while (!iStop) {
                while (GIF_playFrame(&gif, &iDelay, NULL)) {
                    if (iStop) break;
                    ShowFrame(1);
                    usleep(iDelay * 1000);
                }
                if (iStop) break;
                GIF_reset(&gif);
            }
        } else { // Bucle con contador
            for (int i = 0; i < iLoopCount && !iStop; i++) {
                while (GIF_playFrame(&gif, &iDelay, NULL)) {
                    if (iStop) break;
                    ShowFrame(1);
                    usleep(iDelay * 1000);
                }
                if (iStop) break;
                GIF_reset(&gif);
            }
        }
        
        free(pGIFBuf);
        GIF_close(&gif);
    } else {
        fprintf(stderr, "Error al abrir el archivo GIF.\n");
    }
    // Limpieza
    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}

