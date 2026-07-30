#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#include "defs.h"

void runConsole(Player *p) {

  char input[64];

  /* SHUTDOWN 3D */

  cleanupKeyboard();
  cleanupVideo();

  /* COLOR VALUES */

  textbackground(4);
  textcolor(0);
  clrscr();

  /* CONSOLE LOGIC */

  cprintf("\r\n");
  cprintf("========================\r\n");
  cprintf("     M O O N  E N G      \r \n");
  cprintf("========================\r\n");
  cprintf(" \r\n");
  cprintf("type 'help' for commands\r\n");
  cprintf("type 'resume' to return\r\n\r\n");

  while(1) {

    cprintf("MOON> ");

    if (fgets(input, 64, stdin) == NULL) break;    /* wait for user input */

    input[strcspn(input, "\r\n")] = 0;               /* strip hidden new
						      line from the string */

    /* PARSE CMDS */

    if (strcmp(input, "resume") == 0 || strcmp(input, "`") == 0) {

      break;    /* exit */

    }

    else if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {

	textbackground(0);
	textcolor(7);
	clrscr();
	exit(0);

    }

    else if (strcmp(input, "help") == 0) {

      cprintf("\r\nCOMMANDS:\r\n");
      cprintf(" \r\n");
      cprintf("resume             ---     go back to game\r\n");
      cprintf("exit               ---     quit to DOS\r\n");
      cprintf("play_stats         ---     show player config\r\n");
      cprintf("play_pos           ---     show player position\r\n");
      cprintf("play_x             ---     change player x pos\r\n");
      cprintf("play_y             ---     change player y pos\r\n");
      cprintf("play_speed         ---     change player speed\r\n");
      cprintf("play_turn          ---     change player turn speed\r\n");
      cprintf("play_fric          ---     change player friction\r\n");
      cprintf("rend_fog           ---     change fog rendering strength\r\n");
      cprintf("rend_sky           ---     change the sky being rendered\r\n");
      cprintf("map_info           ---     show map info\r\n");
      cprintf("show_fps           ---     DEBUG: show FPS on HUD\r\n");
      cprintf("show_pos           ---     DEBUG: show player position on HUD\r\n");
      cprintf("show_tics          ---     DEBUG: show tics and frame count on HUD\r\n");
      cprintf(" \r\n");

    }

    else if (strcmp(input, "play_pos") == 0) {

      cprintf("PLAYER -> X:%.1f Y:%.1f ANG:%.1f\r\n", p->x, p->y, p->a);

    }

    else if (strcmp(input, "play_stats") == 0) {

      cprintf("CONFIG -> SPD:%.2f TURN:%.2f FRIC:%.2f\r\n", p->speed, p->turnSpeed, p->friction);

    }

    else if (strncmp(input, "rend_fog ", 9) == 0) {

      sscanf(input + 9, "%f", &engineFog);
      cprintf("ENGINE FOG SET TO %.2f\r\n", engineFog);

    }

    else if (strncmp(input, "play_x ", 7) == 0) {

      sscanf(input + 7, "%f", &p->x);
      cprintf("PLAYER X CHANGED TO %.1f\r\n", p->x);

    }

    else if (strncmp(input, "play_y ", 7) == 0) {

      sscanf(input + 7, "%f", &p->y);
      cprintf("PLAYER Y CHANGED TO %.2f\r\n", p->y);

    }

    else if (strncmp(input, "play_speed ", 11) == 0) {

      sscanf(input + 11, "%f", &p->speed);
      cprintf("PLAYER SPEED CHANGED TO %.1f\r\n", p->speed);

    }

    else if (strncmp(input, "play_turn ", 10 ) == 0) {

      sscanf(input + 10, "%f", &p->turnSpeed);
      cprintf("PLAYER TURNSPEED SET TO %.2f\r\n", p->turnSpeed);

    }

    else if (strncmp(input, "play_fric ", 10) == 0) {

      sscanf(input + 10, "%f", &p->friction);
      cprintf("PLAYER FRICTION SET TO %.2f\r\n", p->friction);

    }

    else if (strcmp(input, "show_fps") == 0) {

	    showFPS = !showFPS;
	    cprintf("FPS HUD TOGGLED %s\r\n", showFPS ? "ON" : "OFF");

    }

    else if (strcmp(input, "show_pos") == 0) {

	    showPos = !showPos;
	    cprintf("POS HUD TOGGLED %s\r\n", showPos ? "ON" : "OFF");

    }

    else if (strcmp(input, "show_tics") == 0) {

	    showTics = !showTics;
	    cprintf("TICS HUD TOGGLED %s\r\n", showTics ? "ON" : "OFF");

    }

    else if (strcmp(input, "map_info") == 0) {

      cprintf("MAP -> SIZE: %dx%d SKYBOX: ID %d\r\n", mapWidth, mapHeight, activeSkybox);

    }

    else if (strncmp(input, "rend_sky ", 9) == 0) {

      sscanf(input + 9, "%d", &activeSkybox);
      cprintf("ACTIVE SKYBOX SET TO %d\r\n", activeSkybox);

    }

    else if (strlen(input) > 0) {

      cprintf("UNKNOWN COMMAND: '%s', Type 'help'.\r\n", input);

    }

  }

  /* COLOR RESET */

  textbackground(0);
  textcolor(7);
  clrscr();

  /* RESUME GAME */

  initVideo();
  initKeyboard();

}
