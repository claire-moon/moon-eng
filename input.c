#include <conio.h>
#include <math.h>
#include <pc.h>
#include <stdlib.h>
#include <dpmi.h>
#include <go32.h>

#include "defs.h"

volatile char keys[128] = {0};
_go32_dpmi_seginfo old_kbd, new_kbd;

/* DEFAULT KEYBINDS */

int bindForward		= KEY_W;
int bindBackwards	= KEY_S;
int bindTurnLeft	= KEY_A;
int bindTurnRight	= KEY_D;
int bindStrafeLeft	= KEY_LEFT;
int bindStrafeRight	= KEY_RIGHT;
int bindDash		= KEY_CTRL;
int bindLookUp		= KEY_PGUP;
int bindLookDown	= KEY_PGDN;
int bindCenterView  = KEY_HOME;

void handleKeyboard() {

  unsigned char sc = inportb(0x60);
  if (sc < 128) keys[sc] = 1;
  else keys[sc & 0x7F] = 0;
  outportb(0x20, 0x20);

}

void initKeyboard() {

  int i;

  for(i = 0; i < 128; i++) keys[i] = 0;

  _go32_dpmi_lock_code(handleKeyboard, 1024);
  _go32_dpmi_lock_data((void*)keys, 128);
  _go32_dpmi_get_protected_mode_interrupt_vector(9, &old_kbd);
  new_kbd.pm_offset   = (int)handleKeyboard;
  new_kbd.pm_selector = _go32_my_cs();
  _go32_dpmi_allocate_iret_wrapper(&new_kbd);
  _go32_dpmi_set_protected_mode_interrupt_vector(9, &new_kbd);

}

void cleanupKeyboard() {

  _go32_dpmi_set_protected_mode_interrupt_vector(9, &old_kbd);
  _go32_dpmi_free_iret_wrapper(&new_kbd);

}

void processInput(Player *p) {

  if (keys[KEY_ESC]) {

    cleanupKeyboard();
    cleanupVideo();
    exit(0);

  }

  if (keys[KEY_TILDE]) {

    keys[KEY_TILDE] = 0;
    runConsole(p);

  }

}
