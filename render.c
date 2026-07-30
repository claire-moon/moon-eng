#include <dos.h>
#include <sys/nearptr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pc.h>
#include <math.h>

#include "defs.h"

#define VGA_ADDR 0xA0000

volatile unsigned char *VGA = 0;
unsigned char *VIR_SCR = 0;
float engineFog = 0.05;
int useSkybox = 1;

void waitVsync() {

    while (inportb(0X3DA) & 8);
    while (!(inportb(0x3DA) & 8));

}

void loadMdpPalette() {

    FILE *f;
    MoonHeader head;
    MoonEntry entry;
    int i, found = 0;
    unsigned char palette[768];
    int c, s, r, g, b;
    int base[8][3] = {

    {63, 63, 63},
    {63, 5, 5},
    {5, 63, 5},
    {5, 5, 63},
    {63, 63, 5},
    {5, 63, 63},
    {63, 5, 63},
    {62, 32, 5}

};

    f = fopen(mdpFilename, "rb");

    if (!f) return;

    fread(&head, sizeof(MoonHeader), 1, f);
    fseek(f, head.dirOffset, SEEK_SET);

    for (i = 0; i < head.numLumps; i++) {

        fread(&entry, sizeof(MoonEntry), 1, f);

        if (strcmp(entry.name, "PALETTE") == 0) {

            found = 1;

            break;

        }

    }

    if (found) {

        fseek(f, entry.offset, SEEK_SET);
        fread(palette, 768, 1, f);

        /* PASS TO VGA DAC */

        outportb(0x3C8, 0);

        for (i = 0; i < 768; i++) {

            outportb(0x3C9, palette[i]);
        }

    } else {

        /* FALLBACK PAL GEN */

        outportb(0x3C8, 0);

        for (c = 0; c < 8; c++) {

            for (s = 0; s < 32; s++) {

                r = (base[c][0] * (31 - s)) / 31;
                g = (base[c][1] * (31 - s)) / 31;
                b = (base[c][2] * (31 - s)) / 31;

                outportb(0x3C9, r);
                outportb(0x3C9, g);
                outportb(0x3C9, b);

            }

        }

    }

    if (f) fclose(f);

}


void initVideo() {

    if (__djgpp_nearptr_enable() == 0) exit(1);

    VGA     = (unsigned char *)(__djgpp_conventional_base + VGA_ADDR);
    VIR_SCR = (unsigned char *)malloc(SCR_SIZE);

    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    loadMdpPalette();

}

void cleanupVideo() {

    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
    free(VIR_SCR);
    __djgpp_nearptr_disable();

}

void renderScene(Player *p) {

    int    r, i, lineH, lineO;
    float  ra, dx, dy, dist;
    int    mapX, mapY, stepX, stepY, side;
    float  rayX, rayY, sideDistX, sideDistY, deltaDistX, deltaDistY;
    int    sectorLight, finalShade, shade;
    int    baseH, wallBottom, drawStart, drawEnd, wallColor;
    int    currentLowestTop, visibleEnd;
    int    cameraShift;

    float	 fovRad, halfFovRad, angleStep, projDist;
    float  idlePitch, idleYaw, yawAmp;

    int	 horizonOffset, centerRow;

    int    y, p_row, color;
    float  rowDist;

    /* HEADBOB LOGIC */

    yawAmp    = 0.015;

    if (fabs(p->vx) > 0.1 || fabs(p->vy) > 0.1) {

        yawAmp = 0.030;

    }

    idleYaw	= sin(p->idleTimer) * p->bobAmp;
    idlePitch	= sin(p->idleTimer * 2.0) * 2.0;

    horizonOffset = (int)(p->pitch + idlePitch);

    centerRow = (SCR_H / 2) + horizonOffset;

    if (centerRow < 0) centerRow = 0;
    if (centerRow > SCR_H) centerRow = SCR_H;

    /* FOV LOGIC */

    fovRad		= p->fov * (PI / 180.0);
    halfFovRad	        = fovRad / 2.0;
    angleStep		= fovRad / SCR_W;
    projDist		= (SCR_W / 2.0) / tan(halfFovRad);

    /* CEILING/FLOOR LOGIC */

    for (y = 0; y < SCR_H; y++) {

        p_row = y - centerRow;

        if (p_row < 0) {

            if (useSkybox) {

                drawSkyboxRow(p, y, p_row, idleYaw, activeSkybox);


            } else {

                rowDist = ((TIL_SIZE / 2.0) - p->zOffset) * projDist / abs(p_row);
                shade   = (int)(rowDist * engineFog);

                if (shade < 0) shade = 0;
                if (shade > 31) shade = 31;

                memset(VIR_SCR + (y * SCR_W), shade, SCR_W);

            }

        }

        else if (p_row > 0) {

            rowDist = ((TIL_SIZE / 2.0) - p->zOffset) * projDist / p_row;
            shade   = (int)(rowDist * engineFog);

            if (shade < 0) shade = 0;
            if (shade > 31) shade = 31;

            memset(VIR_SCR + (y * SCR_W), shade, SCR_W);

        }

        else {

            memset(VIR_SCR + (y * SCR_W), 0, SCR_W);

        }

    }

    /* RAYCASTING LOGIC */

    for (r = 0; r < SCR_W; r++) {

        currentLowestTop = SCR_H;

        ra   = (p->a + idleYaw) - halfFovRad + ((float)r * angleStep);
        dx   = cos(ra);
        dy   = sin(ra);

        rayX = p->x / TIL_SIZE;
        rayY = p->y / TIL_SIZE;

        mapX = (int)rayX;
        mapY = (int)rayY;

        deltaDistX = (dx == 0) ? 1e30 : fabs(1.0 / dx);
        deltaDistY = (dy == 0) ? 1e30 : fabs(1.0 / dy);

        if (dx < 0) {

            stepX      = -1;
            sideDistX  = (rayX - mapX) * deltaDistX;

        } else {

            stepX      = 1;
            sideDistX  = (mapX + 1.0 - rayX) * deltaDistX;

        }

        if (dy < 0) {

            stepY       = -1;
            sideDistY   = (rayY - mapY) * deltaDistY;

        } else {

            stepY     = 1;
            sideDistY = (mapY + 1.0 - rayY) * deltaDistY;

        }

        while (1) {

            if (sideDistX < sideDistY) {

                sideDistX += deltaDistX;
                mapX      += stepX;
                side       = 0;

            } else {

                sideDistY += deltaDistY;
                mapY      += stepY;
                side       = 1;

            }

            if (mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight) {

                break;

            }

            int mapTile = currentMap[(mapY * mapWidth) + mapX];

            if (mapTile != 0) {

                if (side == 0) dist = (mapX - rayX + (1 - stepX) / 2.0f) / dx;
                else           dist = (mapY - rayY + (1 - stepY) / 2.0f) / dy;

                dist *= TIL_SIZE;

                /* FISHEYE FIX */

                dist = dist * cos((p->a + idleYaw) - ra);

                if (dist < 1) dist = 1;

                baseH = (TIL_SIZE * projDist) / dist;

                lineH = baseH * mapTile;

                cameraShift = (int)((p->zOffset * projDist) / dist);

                wallBottom = (SCR_H / 2) + (baseH / 2) - cameraShift + horizonOffset;

                lineO = wallBottom - lineH;

                drawStart = lineO;
                drawEnd   = wallBottom;

                if (drawStart < 0) drawStart = 0;
                if (drawStart > SCR_H) drawStart = SCR_H;

                if (drawEnd < 0) drawEnd = 0;
                if (drawEnd > SCR_H) drawEnd = SCR_H;

                wallColor = 0;

                if (mapTile == 1) wallColor = 224;
                if (mapTile == 2) wallColor = 96;
                if (mapTile == 3) wallColor = 32;

                if (drawStart < currentLowestTop) {

                    visibleEnd = drawEnd;

                    if (visibleEnd > currentLowestTop) visibleEnd = currentLowestTop;

                    for (i = drawStart; i < visibleEnd; i++) {

                        shade = (int)(dist * engineFog);

                        /* DITHERING ALGO */

                        if (side == 1) shade -= 4;
                        if ((r+i) & 1) shade -= 2;

                        if (shade < 0) shade = 0;
                        if (shade > 31) shade = 31;

                        VIR_SCR[(i * SCR_W) + r] = wallColor + shade;

                    }

                    currentLowestTop = drawStart;

                    if (currentLowestTop <= 0) break;

                }

            }

        }

    }

    drawHUD(p);

    waitVsync();

    memcpy((void *)VGA, VIR_SCR, SCR_SIZE);

}
