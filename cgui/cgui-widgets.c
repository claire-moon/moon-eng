/* TODO :

   - up and down arrow func with listbox
   - fix text truncator to properly display "..."
   - fix max char in listbox (equal to our less than 8)
   - arrow buttons for scroll bar
   - canvas widget
   - grid map widget/windo
   - keyboard controls gui wide
   - kill self
   
*/

#include "cgui-widgets.h"
#include "cgui.h"

#include <string.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

int addWidget(int winIdx, WidgetType type,
              int x, int y, int w, int h, char *text) {

    Window *win;
    Widget *w_new;

    if (winIdx < 0 || winIdx >= windowCount)
        return -1;

    win = &windows[winIdx];

    if (win->widgetCount >= WIDGETS_MAX)
        return -1;

    w_new        = &win->widgets[win->widgetCount];
    w_new->id    = win->widgetCount;
    w_new->type  = type;
    w_new->x     = x;
    w_new->y     = y;
    w_new->w     = w;
    w_new->h     = h;

    w_new->parentWin = winIdx;

    if (text)
        strcpy(w_new->text, text);

    w_new->isHovered = 0;
    w_new->isFocused = 0;
    w_new->onClick   = NULL;

    w_new->val   = 0;
    w_new->min   = 0;
    w_new->max   = 100;

    if (type == WIDGET_CONSOLE) {

        int i;

        for (i = 0; i < 25; i++) {

            w_new->consoleLines[i][0] = '\0';

        }

        w_new->consoleCursorY = 0;

    }

    w_new->val = 0;
    w_new->min = 0;
    w_new->max = 100;

    win->widgetCount++;

    return win->widgetCount - 1;
  
}

void drawWidget(Widget *w, int winX, int winY) {

    int absX = winX + w->x;
    int absY = winY + w->y;
    int textLen, cursorX;
    int boxSize, i, itemY, itemIdx, visibleItems;
    int trackH, thumbH, thumbY;
    int maxChars, extLen, keepBase, dotCount;
    int gx, gy, c;


    char *orig;
    char *ext;
    char shortName[64];
  
    float scrollPct;
  
    if (w->type == WIDGET_LABEL) {

        drawString(absX, absY, w->text, 0);
    
    }

    else if (w->type == WIDGET_BUTTON) {

        /* BTN BG */

        drawRect(absX, absY, w->w, w->h, 154);

        /* PRESSED STATES */

        if (w->isPressed) {

            drawBorder(absX, absY, w->w, w->h, 0, 248);

            drawString(absX + 7, absY + 5, w->text, 0);
      
        }

        else {

            drawBorder(absX, absY, w->w, w->h, 248, 0);
      
            drawString(absX + 6, absY + 4, w->text, 0);

        }
   
    }

    else if (w->type == WIDGET_TOGGLE) {

        /* BTN BG */

        drawRect(absX, absY, w->w, w->h, 154);

        /* PRESSED STATES */

        if (w->isChecked || w->isPressed) {

            drawBorder(absX, absY, w->w, w->h, 0, 248);

            drawString(absX + 7, absY + 5, w->text, 0);
      
        }

        else {

            drawBorder(absX, absY, w->w, w->h, 240, 0);
      
            drawString(absX + 6, absY + 4, w->text, 0);

        }
   
    }

    /* CHECKBOX LOGIC */
  
    else if (w->type == WIDGET_CHECKBOX) {

        boxSize = 10;

        drawRect(absX, absY, boxSize, boxSize, 248);

        /* CHECKBOX BORDERS */

        drawBorder(absX, absY, boxSize, boxSize, 0, 154);

        if (w->isChecked) {

            /* DRAW CHECKMARK */
      
            drawLine(absX + 2, absY + 5, absX + 4, absY + 7, 0);
            drawLine(absX + 4, absY + 7, absX + 8, absY + 3, 0);
      
        }

        drawString(absX + boxSize + 6, absY + 2, w->text, 0);
    
    }

    /* TEXT INPUT FIELD */

    else if (w->type == WIDGET_INPUT) {

        /* FIELD BG */
    
        drawRect(absX, absY, w->w, w->h, 248);

        /* BORDERS */

        drawBorder(absX, absY, w->w, w->h, 0, 154);

        /* DRAW TEXT I/O */
    
        drawString(absX + 4, absY + 4, w->text, 0);

        /* DRAW CARET */

        /* will only draw if focused -- blinks every 15 frames */

        if (w->isFocused && (frameCount % 30 < 15)) {

            textLen = strlen(w->text);
            cursorX = absX + 4 + (textLen * 4);

            /* make sure it dont leave that box */

            if (cursorX < absX + w->w - 4) {

                drawLine(cursorX, absY + 3, cursorX, absY + 11, 0);

            }

        }

    }

    else if (w->type == WIDGET_LISTBOX) {
    
        visibleItems = (w->h - 4) / 10;

        drawBorder(absX, absY, w->w, w->h, 0, 154);

        drawRect(absX + 1, absY + 1, w->w - 2, w->h - 2, 0);

        if (w->listCount > visibleItems) {

            setClip(absX + 2, absY + 2, w->w - 14, w->h - 4);
      
        } else {

            setClip(absX + 2, absY + 2, w->w - 4, w->h - 4);
      
        }
    
        /* DRAW VISIBLE ITEMS */

        for (i = 0; i < visibleItems; i++) {

            itemIdx = i + w->listScroll;

            if (itemIdx >= w->listCount) break;

            itemY = absY + 2 + (i * 10);

            /* TRUNCATE FILE NAME */

            {

                maxChars = (w->w - 18) / 4;
                orig = w->listItems[itemIdx];
                ext = strchr(orig, '.');

                if (strlen(orig) <= maxChars) {

                    strcpy(shortName, orig);
	  
                } else if (ext != NULL && maxChars > strlen(ext) + 1) {

                    extLen = strlen(ext);
                    keepBase = maxChars - extLen - 2;
                    dotCount = 2;

                    if (keepBase < 1) {

                        keepBase = 1;
                        dotCount = maxChars - extLen - 1;

                    }

                    strncpy(shortName, orig, keepBase);

                    shortName[keepBase] = '\0';

                    while(dotCount-- > 0) strcat(shortName, ".");

                    strcat(shortName, ext);

                } else {

                    strncpy(shortName, orig, maxChars);

                    shortName[maxChars] = '\0';

                }

            }
      
            /* DRAW SELECTION */

            if (itemIdx == w->listSelected) {

                drawRect(absX + 2, itemY - 1, w->w - 4, 10, 1);
                drawString(absX + 4, itemY, w->listItems[itemIdx], 248);

            } else {

                drawString(absX + 4, itemY, w->listItems[itemIdx], 0);

            }

        }

        resetClip();

        /* DRAW SCROLLBAR */

        drawRect(absX + w->w - 12, absY + 2, 10, w->h - 4, 154);

        /* DRAW THUMBS */

        if (w->listCount > visibleItems) {

            trackH = w->h - 4;
            scrollPct = (float)w->listScroll / (w->listCount - visibleItems);

            thumbH = (visibleItems * trackH) / w->listCount;

            if (thumbH < 8) thumbH = 8;

            thumbY = absY + 2 + (int)(scrollPct * (trackH - thumbH));

            drawRect(absX + w->w - 12, thumbY, 10, thumbH, 154);
            drawRect(absX + w->w - 12, thumbY + thumbH - 1, 10, 1, 0);
            drawRect(absX + w->w - 3, thumbY, 1, thumbH, 0);
            drawRect(absX + w->w - 12, thumbY, 10, 1, 248);
            drawRect(absX + w->w - 12, thumbY, 1, thumbH, 248);

        }
      
    } else if (w->type == WIDGET_SLIDER) {

        int range, thumbW, thumbX, trackY;
        float pct;

        range = w -> max - w->min;
        pct = (range != 0) ? (float)(w->val - w->min) / (float)range : 0.0f;

        if (pct < 0.0f) pct = 0.0f;
        if (pct > 1.0f) pct = 1.0f;

        thumbW = 8;
        thumbX = absX + (int)(pct * (w->w - thumbW));
        trackY = absY + (w->h / 2);

        drawRect(absX, trackY - 2, w->w, 4, 154);
        drawRect(absX, trackY - 2, w->w, 1, 0);
        drawRect(absX, trackY + 1, w->w, 1, 248);

        drawRect(thumbX, absY, thumbW, w->h, 154);
        drawRect(thumbX, absY, thumbW, 1, 148);
        drawRect(thumbX, absY, 1, w->h, 248);
        drawRect(thumbX, absY + w->h - 1, thumbW, 1, 0);
        drawRect(thumbX + thumbW - 1, absY, 1, w->h, 0);

    } else if (w->type == WIDGET_CANVAS_IMG) {

        cguiImage *img;
        int px, py;

        drawBorder(absX, absY, w->w, w->h, 0, 154);

        if (w->data != NULL) {

            img = (cguiImage *)w->data;

            setClip(absX + 1, absY + 1, w->w - 2, w->h - 2);

            for (py = 0; py < img->h; py++) {

                for (px = 0; px < img->w; px++) {

                    drawPixel(absX + 1 + px, absY + 1 + py, img->pixels[py * img->w + px]);

                }

            }

            resetClip();

        }

    } else if (w->type == WIDGET_CANVAS_GRID) {


        int gx, gy, c;

        cguiGrid *grid;

        drawBorder(absX, absY, w->w, w->h, 0, 154);

        if (w->data != NULL) {

            grid = (cguiGrid *)w->data;

            setClip(absX + 1, absY + 1, w->w - 2, w->h - 2);

            drawRect(absX + 1, absY + 1, w->w - 2, w->h - 2, 0);

            if (grid->cells) {

                for (gy = 0; gy < grid->rows; gy++) {

                    for (gx = 0; gx < grid->cols; gx++) {

                        c = grid->cells[gy * grid->cols + gx];

                        if (c != 0) {

                            drawRect(absX + 1 + (gx * grid->tileSize),
                                     absY + 1 + (gy * grid->tileSize),
                                     grid->tileSize, grid->tileSize, c);

                        }

                    }

                }

            }

            for (gx = 0; gx <= grid->cols * grid->tileSize;
                 gx += grid->tileSize) {

                drawRect(absX + 1 + gx, absY + 1, 1, grid->rows * grid->tileSize, 10);

            }


            for (gy = 0; gy <= grid->cols * grid->tileSize;
                 gy += grid->tileSize) {

                drawRect(absX + 1, absY + 1 + gy, grid->cols * grid->tileSize, 1, 10);

            }

            if (grid->selX >= 0 && grid->selY >= 0) {

                drawBorder(absX + 1 + (grid->selX * grid->tileSize),
                           absY + 1 + (grid->selY * grid->tileSize),
                           grid->tileSize + 1, grid->tileSize + 1, 248, 248);

            }

            resetClip();

        }

    } else if (w->type == WIDGET_CONSOLE) {

        int i;

        /* BG - WHITE */

        drawRect(absX, absY, w->w, w->h, 248);

        /* BORDER */

        drawBorder(absX, absY, w->w, w->h, 0, 154);

        /* LOCK DRAWING IN THA BOX */

        setClip(absX + 2, absY + 2, w->w - 4, w->h - 4);

        for (i = 0; i < 25; i++) {

            if (w->consoleLines[i][0] != '\0') {

                drawString(absX + 4, absY + 4 + (i * 10), w->consoleLines[i], 0);

            }

        }

        resetClip();

    }
    
}
