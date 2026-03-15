#include <time.h>
#include <stdio.h>

#include "cgui.h"
#include "cgui-pal.h"
#include "cgui-font.h"

/* GLOBALS */

unsigned char *VGA = (unsigned char *)0xA0000;
unsigned char *VIR_SCR = NULL;

int mouseX = 160;
int mouseY = 100;
int mouseB = 0;
int appRunning = 1;
int prevMouseB = 0;
int windowCount = 0;
int topWindowIdx = -1;
int activeDropdown = 0;
int isTyping = 1;
int clipX1 = 0;
int clipY1 = 0;
int clipX2 = SCR_W - 1;
int clipY2 = SCR_H - 1;

unsigned long frameCount = 0;
unsigned long lastClickFrame = 0;

char inputBuffer[32];

Window windows[MAX_WINDOWS];
MenuCategory sysMenu[MAX_MENU_CATEGORIES];

int zOrder[MAX_WINDOWS];
int cguiDebugMode = 1;
int sysMenuCount = 0;
int sysBgMode = 0;
int sysBgColor = 80;

unsigned char sysBgGradient[200];

StatusItem statusLeft = { STATUS_EMPTY, "", NULL, 0, NULL, 0, 0 };
StatusItem statusRight = { STATUS_EMPTY, "", NULL, 0, NULL, 0, 0 };

void setBackgroundSolid(int color) {

    sysBgMode = 0;
    sysBgColor = color;

}

void setBackgroundGradient(int c1, int c2) {

    int y, i;

    int r1 = cgui_palette[c1 * 3], g1 = cgui_palette[c1 * 3 + 1],
        b1 = cgui_palette[c1 * 3 + 2];

    int r2 = cgui_palette[c2 * 3], g2 = cgui_palette[c2 * 3 + 1],
        b2 = cgui_palette[c2 * 3 + 2];

    sysBgMode = 1;

    for (i = 0; i < 64; i++) {

        float pct = (float)i / 63.0f;

        int tr = r1 + (int)((r2 - r1) * pct);
        int tg = g1 + (int)((g2 - g1) * pct);
        int tb = b1 + (int)((b2 - b1) * pct);

        outportb(0x3C8, 180 + i);
        outportb(0x3C9, tr);
        outportb(0x3C9, tg);
        outportb(0x3C9, tb);

    }

    for (y = 0; y < 200; y++) {

        int colorStep = (y * 63) / 199;

        sysBgGradient[y] = 180 + colorStep;

    }

}

void drawDesktop() {

    if (sysBgMode == 0) {

        memset(VIR_SCR, sysBgColor, 64000);

    } else {

        int y;

        for (y = 0; y < 200; y++) {

        memset(VIR_SCR + (y * 320), sysBgGradient[y], 320);

        }

    }

}

void initCGUIPalette() {

    int i;

    outportb(0x3C8, 0);

    for (i = 0; i < 768; i++) {

        outportb(0x3C9, cgui_palette[i]);
    
    }
  
}

/* MOUSE DRIVER */

void updateMouse() {

    union REGS r;
    r.x.ax = 3;
    int86(0x33, &r, &r);

    mouseX = (short)r.x.cx / 2;
    mouseY = (short)r.x.dx;

    prevMouseB = mouseB;

    mouseB = r.x.bx;

    if (mouseX < 0)
        mouseX = 0;

    if (mouseX > 319)
        mouseX = 319;

    if (mouseY < 0)
        mouseY = 0;

    if (mouseY > 199)
        mouseY = 199;
}

void updateKeyboard() {

    int i, j;
    int len;
  
    char c;

    Widget *focused = NULL;

    if (!isTyping) return;

    /* FIND FOCUSED WIDGET */

    for (i = 0; i < windowCount; i++) {

        for (j = 0; j < windows[i].widgetCount; j++) {

            if (windows[i].widgets[j].isFocused) {

                focused = &windows[i].widgets[j];

                break;

            }
      
        }
    
    }

    if (focused == NULL) return;

    while (kbhit()) {

        c = getch();
        len = strlen(focused->text);

        /* BACKSPACE */

        if (c == 8 && len > 0) {

            focused->text[len - 1] = '\0';

        }

        /* ENTER */

        /* TODO: defocuses the box (for now) come back when
           working on focusing logic */

        else if (c == 13) {

            focused->isFocused = 0;

            /* passes an onClick() to emulate submit (for now) */

            if (focused->onClick != NULL) focused->onClick(focused);

        }

        /* GENERAL TYPE */

        else if (c >= 31 && c <= 126 && len < 30) {

            if ((len * 4) + 8 < focused-> w) {

                focused->text[len]     = c;
                focused->text[len + 1] = '\0';

            }

        }

    }

}
  
/* GFX PRIMATIVES */

void setClip(int x, int y, int w, int h) {

    clipX1 = x;
  
    if (clipX1 < 0) clipX1 = 0;

    clipY1 = y;

    if (clipY1 < 0) clipY1 = 0;

    clipX2 = x + w - 1;

    if (clipX2 >= SCR_W) clipX2 = SCR_W - 1;

    clipY2 = y + h - 1;

    if (clipY2 >= SCR_H) clipY2 = SCR_H - 1;
  
}

void resetClip() {

    clipX1 = 0;
    clipY1 = 0;
    clipX2 = SCR_W - 1;
    clipY2 = SCR_H - 1;
  
}

void drawPixel(int x, int y, int color) {

    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y < clipY2) {

        VIR_SCR[y * SCR_W + x] = color;
    
    }
  
}

void drawLine(int x0, int y0, int x1, int y1, int color) {

    int dx  = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy  = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {

        drawPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        e2 = 2 * err;

        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    
    }
  
}

void drawRect(int x, int y, int w, int h, int color) {

    int i, j;

    for (j = 0; j < h; j++) {

        if (y + j < clipY1 || y + j >= clipY2)
            continue;

        for (i = 0; i < w; i++) {

            if (x + i < clipX1 || x + i >= clipX2)
                continue;

            VIR_SCR[(y + j) * SCR_W + (x + i)] = color;
        }
    }
}

void drawChar(int x, int y, char c, int color) {


    char *p;

    int idx = 36;
    int i, j, row;

    if (c >= 'A' && c <= 'Z')
        idx = c - 'A';

    else if (c >= '0' && c <= '9')
        idx = c - '0' + 26;

    else if (c >= 'a' && c <= 'z')
        idx = c - 'a' + 70;

    else {

        p = strchr(cguiSyms, c);

        if (p) idx = 37 + (p - cguiSyms);
    
    }

    /* DRAW THE DANG PIXELS ! */

    for (j = 0; j < 5; j++) {

        row = cguiFont[idx][j];

        for (i = 0; i < 3; i++) {

            if (row & (1 << (2 - i))) {

                drawPixel(x + i, y + j, color);

            }
        }
    }
}

void drawString(int x, int y, char *str, int color) {

    int i = 0;

    while (str[i] != '\0') {

        if (x + (i * 4) + 2 > clipX2) break;

        drawChar(x + (i * 4), y, str[i], color);

        i++;
    }
}

void initWindowManager() {

    int i;

    for (i = 0; i < MAX_WINDOWS; i++) {

        windows[i].visible = 0;
        windows[i].inUse = 0;
        zOrder[i] = -1;
    
    }

    windowCount = 0;
  
}

int createWindow(int x, int y, int w, int h, char *title) {

    int i, idx = -1;

    for (i = 0; i < MAX_WINDOWS; i++) {

        if (!windows[i].inUse) {

            idx = i;

            break;
      
        }
    
    }

    if (idx == -1) return -1;

    windows[idx].id = idx;
    windows[idx].x  = x;
    windows[idx].y  = y;
    windows[idx].w  = w;
    windows[idx].h  = h;

    strcpy(windows[idx].title, title);

    windows[idx].visible = 1;
    windows[idx].inUse = 1;
    windows[idx].dragging = 0;
    windows[idx].titleColor = 1;
    windows[idx].widgetCount = 0;

    for (i = windowCount; i > 0; i--) {

        zOrder[i] = zOrder[i - 1];
    
    }

    zOrder[0] = idx;

    windowCount++;

    return idx;
  
}

void destroyWindow(int id) {

    int i, j;
  
    if (!windows[id].inUse) return;

    windows[id].inUse = 0;
    windows[id].visible = 0;

    for (i = 0; i < windowCount; i++) {

        if (zOrder[i] == id) {

            for (j = i; j < windowCount - 1; j++) {

                zOrder[j] = zOrder[j + 1];

            }

            break;
      
        }
    
    }

    windowCount--;
  
}

void bringToFront(int id) {

    int i, j;

    for (i = 0; i < windowCount; i++) {

        if (zOrder[i] == id) {

            for (j = i; j > 0; j--) {

                zOrder[j] = zOrder[j - 1];
	
            }

            zOrder[0] = id;

            break;
      
        }
    
    }
  
}

void drawWindow(Window *w) {

    if (!w->visible)
        return;

    /* MAIN BODY */

    drawRect(w->x, w->y, w->w, w->h, 154);

    /* BORDER */

    drawRect(w->x, w->y, w->w, 1, 248);
    drawRect(w->x, w->y, 1, w->h, 248);
    drawRect(w->x, w->y + w->h - 1, w->w, 1, 0);
    drawRect(w->x + w->w - 1, w->y, 1, w->h, 0);

    /* TITLE BAR */

    drawRect(w->x + 2, w->y + 2, w->w - 4, 10, w->titleColor);

    /* CLOSE BUTTON */

    drawRect(w->x + w->w - 12, w->y + 3, 8, 8, 4);

    /* DRAW ALL AVAILABLE WIDGETS */

    for (int i = 0; i < w->widgetCount; i++) {

        drawWidget(&w->widgets[i], w->x, w->y);
      
    }
    
    /* TITLE TEXT */
    
    drawString(w->x + 4, w->y + 4, w->title, 248);
}

int addMenuCategory(char *name) {

    if (sysMenuCount >= MAX_MENU_CATEGORIES)
        return -1;

    strcpy(sysMenu[sysMenuCount].name, name);
    sysMenu[sysMenuCount].itemCount = 0;

    /* auto-calc the X pos
       (based on prev menu width) */

    if (sysMenuCount == 0) sysMenu[sysMenuCount].x = 4;

    else sysMenu[sysMenuCount].x = sysMenu[sysMenuCount - 1].x +
            sysMenu[sysMenuCount - 1].w + 16;

    sysMenu[sysMenuCount].w = strlen(name) * 4;

    return sysMenuCount++;

}

void addMenuItem(int catIdx, char *name, void (*onClick)(void)) {

    int itemIdx, newW;

    if (catIdx < 0 || catIdx >= sysMenuCount)
        return;

    itemIdx = sysMenu[catIdx].itemCount;

    if (itemIdx >= MAX_MENU_ITEMS) return;

    strcpy(sysMenu[catIdx].items[itemIdx].name, name);

    sysMenu[catIdx].items[itemIdx].onClick = onClick;
    sysMenu[catIdx].items[itemIdx].isTogglable = 0;
    sysMenu[catIdx].items[itemIdx].isChecked = 0;

    sysMenu[catIdx].itemCount++;

    newW = (strlen(name) * 4) + 24;

    if (newW > sysMenu[catIdx].dropW) sysMenu[catIdx].dropW = newW;

}

void addMenuItemToggle(int catIdx, char *name, void (*onClick)(void)) {

    int itemIdx, newW;

    if (catIdx < 0 || catIdx >= sysMenuCount)
        return;

    itemIdx = sysMenu[catIdx].itemCount;

    if (itemIdx >= MAX_MENU_ITEMS) return;

    strcpy(sysMenu[catIdx].items[itemIdx].name, name);

    sysMenu[catIdx].items[itemIdx].onClick = onClick;
    sysMenu[catIdx].items[itemIdx].isTogglable = 1;
    sysMenu[catIdx].items[itemIdx].isChecked = 0;

    sysMenu[catIdx].itemCount++;

    newW = (strlen(name) * 4) + 40;

    if (newW > sysMenu[catIdx].dropW) sysMenu[catIdx].dropW = newW;

}

void cguiToggleDebug() {

    cguiDebugMode = !cguiDebugMode;

}

void cguiExit() {

    appRunning = 0;

}

void drawDebugOverlay() {

    int i;

    if (!cguiDebugMode) return;

    drawRect(0, 12, 120, 24 + (windowCount * 10), 0);
    drawString(2, 14, "SYS WINDOWS: ", 5);
    drawChar(50, 14, windowCount + '0', 5);

    drawString(2, 24, "Z:  ID:  TITLE:", 7);

    for (i = 0; i < windowCount; i++) {

        drawChar(2, 34 + (i * 10), i + '0', 8);
        drawChar(18, 34 + (i * 10), windows[zOrder[i]].id + '0', 8);
        drawString(34, 34 + (i * 10), windows[zOrder[i]].title, 8);

    }

}

void setStatusLeft(StatusType type, char *text, char *dynPtr, int isClickable,
                   void (*onClick)(void)) {

    statusLeft.type = type;
    if (text)
        strcpy(statusLeft.text, text);
    statusLeft.dynPtr = dynPtr;
    statusLeft.isClickable = isClickable;
    statusLeft.onClick = onClick;

}

void setStatusRight(StatusType type, char *text, char *dynPtr, int isClickable,
                   void (*onClick)(void)) {

    statusRight.type = type;
    if (text)
        strcpy(statusRight.text, text);
    statusRight.dynPtr = dynPtr;
    statusRight.isClickable = isClickable;
    statusRight.onClick = onClick;

}

void renderStatusItem(StatusItem *item, int isLeft) {

    char buf[64];
    int bg = 154, color = 0;

    if (item->type == STATUS_EMPTY)
        return;

    buf[0] = '\0';

    if (item->type == STATUS_STRING) {

        strcpy(buf, item->text);

    } else if (item->type == STATUS_DYNAMIC && item->dynPtr != NULL) {

        strcpy(buf, item->dynPtr);

    } else if (item->type == STATUS_CLOCK || item->type == STATUS_DATE) {

        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        if (item->type == STATUS_CLOCK) {

            int h = tm->tm_hour % 12;

            if (h == 0)
                h = 12;

            sprintf(buf, "%d:%02d %s", h, tm->tm_min, tm->tm_hour >= 12 ? "PM" : "AM");

        } else {

            sprintf(buf, "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100);

        }

    }

    item->w = strlen(buf) * 4;
    item->x = isLeft ? 4 : SCR_W - item->w - 4;

    if (item->isClickable && mouseY >= 189 && mouseX >= item->x &&
        mouseX <= item->x + item->w) {

      bg = 0;
      color = 248;

      drawRect(item->x - 2, 190, item->w + 4, 9, bg);

    }

    drawString(item->x, 192, buf, color);

}

void drawStatusBar(void) {

    drawRect(0, 189, SCR_W, 11, 154);
    drawRect(0, 189, SCR_W, 1, 248);
    renderStatusItem(&statusLeft, 1);
    renderStatusItem(&statusRight, 0);

}

void drawMenuBar() {

    int i;

    /* BACKGROUND BAR */

    drawRect(0, 0, SCR_W, 11, 154);

    /* SHADOW */

    drawRect(0, 11, SCR_W, 1, 0);

    /* MENU ITEMS */

    for (i = 0; i < sysMenuCount; i++) {

        if (activeDropdown == i + 1) {

            drawRect(sysMenu[i].x - 4, 1, sysMenu[i].w + 8, 10, 0);
            drawString(sysMenu[i].x, 4, sysMenu[i].name, 248);

        } else {

            drawString(sysMenu[i].x, 4, sysMenu[i].name, 0);

        }

    }

}

void drawDropdown() {

    int catIdx, dropX, dropY = 11, dropW, dropH, i;
    int checkX;

    if (activeDropdown == 0)
        return;

    catIdx = activeDropdown - 1;
    dropX = sysMenu[catIdx].x - 4;

    if (sysMenu[catIdx].itemCount == 0) {

        dropW = 52;
        dropH = 20;

    } else {

    dropW = sysMenu[catIdx].dropW;
    dropH = sysMenu[catIdx].itemCount * 10 + 10;

    }

    drawRect(dropX, dropY, dropW, dropH, 154);
    drawRect(dropX, dropY, dropW, 1, 248);
    drawRect(dropX, dropY, 1, dropH, 248);
    drawRect(dropX, dropY + dropH - 1, dropW, 1, 0);
    drawRect(dropX + dropW - 1, dropY, 1, dropH, 0);

    if (sysMenu[catIdx].itemCount == 0) {

        drawString(dropX + 6, dropY + 6, "(EMPTY)", 8);

        return;

    }

    for (i = 0; i < sysMenu[catIdx].itemCount; i++) {

        if (mouseX >= dropX && mouseX <= dropX + dropW &&
            mouseY >= dropY + 5 + (i * 10) && mouseY < dropY + 15 + (i * 10)) {

            drawRect(dropX + 2, dropY + 4 + (i * 10), dropW - 4, 10, 1);
            drawString(dropX + 6, dropY + 6 + (i * 10), sysMenu[catIdx].items[i].name, 248);

        } else {

            drawString(dropX + 6, dropY + 6 + (i * 10), sysMenu[catIdx].items[i].name, 0);

        }

        if (sysMenu[catIdx].items[i].isTogglable) {

            checkX = dropX + dropW - 14;

            drawRect(checkX, dropY + 5 + (i * 10), 8, 8, 248);
            drawRect(checkX, dropY + 5 + (i * 10), 8 , 1, 0);
            drawRect(checkX, dropY + 5 + (i * 10), 1, 8, 0);
            drawRect(checkX, dropY + 12 + (i * 10), 8, 1, 154);
            drawRect(checkX + 7, dropY + 5 + (i * 10), 1, 8, 154);

            if (sysMenu[catIdx].items[i].isChecked) {

                drawString(checkX + 2, dropY + 6 + (i * 10), "x", 0);

            }

        }

    }

}

void drawCursor() {

    int x = mouseX;
    int y = mouseY;
    int i;
    int idx;

    for (i = -3; i <= 3; i++) {

        if (x + i >= 0 && x + i < SCR_W) {

            idx = y * SCR_W + (x + i);
            VIR_SCR[idx] = (VIR_SCR[idx] == 248) ? 0 : 248;

        }

        if (i != 0 && y + i >= 0 && y + i < SCR_H) {

            idx = (y + i) * SCR_W + x;
            VIR_SCR[idx] = (VIR_SCR[idx] == 248) ? 0 : 248;

        }

    }

}

void waitVsync() {

    while (inportb(0x3DA) & 8);
    while (!(inportb(0x3DA) & 8));
  
}

void updateGUI() {

    int i, j;

    Window *w;

    Widget *wid;
  
    int click = (mouseB & 1) && !(prevMouseB & 1);
    int hold = (mouseB & 1);
    int release = !(mouseB & 1) && (prevMouseB & 1);
    int doubleClick = 0;
    int absX, absY, clickedRow, targetIdx;
    int visible, clickedItem;

    float pct;
    
    /* DOUBLE CLICK TRACKER */

    frameCount++;

    if (click) {

        if (frameCount - lastClickFrame < 15) {

            doubleClick = 1;

            lastClickFrame = 0;

        } else {

            lastClickFrame = frameCount;
      
        }
    
    }

    /* CLEAR FOCUS ON CLICK*/
  
    if (click) {

        for (i = 0; i < windowCount; i++) {

            for (j = 0; j < windows[i].widgetCount; j++) {

                windows[i].widgets[j].isFocused = 0;

            }
      
        }
    
    }
  
    /* DRAG HANDLER */

    for (i = 0; i < windowCount; i++) {

        if (windows[i].dragging) {

            if (!hold) {

                windows[i].dragging = 0;

            } else {

                windows[i].x = mouseX - (windows[i].w / 2);
                windows[i].y = mouseY - 5;

            }
      
        }
    
    }

    /* DROPDOWNS */

    if (click) {

        if (activeDropdown) {

            int catIdx = activeDropdown - 1;
            int dropX = sysMenu[catIdx].x - 4;
            int dropW = sysMenu[catIdx].dropW;
            int dropY = 11;
            int dropH = sysMenu[catIdx].itemCount * 10 + 10;

            if (mouseX >= dropX && mouseX <= dropX + dropW && mouseY >= dropY &&
                mouseY <= dropY + dropH) {

                clickedItem = (mouseY - dropY - 5) / 10;

                if (clickedItem >= 0 &&
                    clickedItem < sysMenu[catIdx].itemCount) {

                    if (sysMenu[catIdx].items[clickedItem].isTogglable) {

                        sysMenu[catIdx].items[clickedItem].isChecked =
                            !sysMenu[catIdx].items[clickedItem].isChecked;

                  }

                    if (sysMenu[catIdx].items[clickedItem].onClick != NULL) {

                        sysMenu[catIdx].items[clickedItem].onClick();

                  }

                }

            }

            activeDropdown = 0;
            return;

        }

        if (mouseY <= 11) {

            for (i = 0; i < sysMenuCount; i++) {

              if (mouseX >= sysMenu[i].x - 4 &&
                  mouseX <= sysMenu[i].x + sysMenu[i].w + 4) {

                  activeDropdown = i + 1;
                  return;

              }

          }

        }

    }

    if (mouseY >= 189) {

        if (click) {

          if (statusLeft.isClickable && mouseX >= statusLeft.x &&
              mouseX <= statusLeft.x + statusLeft.w) {

              if (statusLeft.onClick) statusLeft.onClick();

          }

          if (statusRight.isClickable && mouseX >= statusRight.x &&
              mouseX <= statusRight.x + statusRight.w) {

              if (statusRight.onClick) statusRight.onClick();

            }

        }

        return;

    }

    /* WINDOW CHECKER */

    for (i = 0; i < windowCount; i++) {

        w = &windows[zOrder[i]];

        if (!w->inUse || !w->visible)
            continue;

        /* FIXED BOUNADRIES */

        if (mouseX >= w->x && mouseX <= w->x + w->w && mouseY >= w->y &&
            mouseY < w->y + w->h) {

            /* INIT WIDGET LOOP */

            for (j = 0; j < w->widgetCount; j++) {

                wid = &w->widgets[j];

                absX = w->x + wid->x;
                absY = w->y + wid->y;

                if (mouseX >= absX && mouseX <= absX + wid->w &&
                    mouseY >= absY && mouseY < absY + wid->h) {

                    wid->isHovered = 1;

                    if (click && wid->type == WIDGET_INPUT) wid->isFocused = 1;

                    if (wid->type == WIDGET_LISTBOX) {

                        visible = (wid->h - 4) / 10;

                        if (mouseX > absX + wid->w - 12) {

                            if (hold) {

                                pct = (float)(mouseY - (absY + 2)) / (wid->h - 4);

                                if (pct < 0.0) pct = 0.0;

                                if (pct > 1.0) pct = 1.0;

                                if (wid->listCount > visible) {

                                    wid->listScroll = (int)(pct * (wid->listCount - visible));
		  
                                }
		
                            }
	      
                        } else if (click) {

                            clickedRow = (mouseY - absY - 2) / 10;
                            targetIdx = wid->listScroll + clickedRow;

                            if (targetIdx >= 0 && targetIdx <
                                wid->listCount && clickedRow < visible) {

                                if (wid->listSelected == targetIdx) wid->listSelected = -1;
                                else wid->listSelected = targetIdx;
                                if (wid->onClick != NULL) wid->onClick(wid);
		
                            } else {

                                wid->listSelected = -1;
		
                            }
	      
                        }
	    
                    }
	  
                    if (hold) {

                        /* BUTTON PUSH */

                        wid->isPressed = 1;

                        if (wid->type == WIDGET_SLIDER) {

                            pct = (float)(mouseX - absX) / (float)wid->w;

                            if (pct < 0.0f)
                                pct = 0.0f;

                            if (pct > 1.0f)
                                pct = 0.0f;

                            wid->val = wid->min + wid->min +
                                (int) + (int)(pct * (wid->max - wid->min));

                            if (wid->onClick != NULL) wid->onClick(wid);

                        }


                    } else {

                        /* essentially once the mouse is released
                           the system registers it as a click */

                        if (release && wid->isPressed) {

                            if (wid->type == WIDGET_BUTTON) {

                                if (wid->onClick != NULL) wid->onClick(wid);
		
                            }

                            else if (wid->type == WIDGET_TOGGLE ||
                                     wid->type == WIDGET_CHECKBOX) {

                                wid->isChecked = !wid->isChecked;

                                if (wid->onClick != NULL) wid->onClick(wid);
		
                            }
		      
                        }
		      
                        wid->isPressed = 0;   /* pop it back out */

                    }
	    
                } else {

                    /* mouse dragged off the button */

                    wid->isHovered = 0;
                    wid->isPressed = 0;
		  
                }

            }
				
            /* END WIDGET LOOP */
		
            if (click) {
	      
                /* Z - ORDER */

                bringToFront(w->id);

                /* CLOSE BUTTON CLICKED */

                if (mouseX > w->x + w->w - 12 && mouseY < w->y + 12) {

                    destroyWindow(w->id);
                    return;
	  
                }

                /* TITLE BAR CLICKED */

                else if (mouseY < w->y + 12) {

                    if (doubleClick) {

                        strcpy(w->title, "THAT TICKLES..!");
	    
                    }

                    w->dragging = 1;
		    
                }
            }

            return;
        }
    }
}

  





