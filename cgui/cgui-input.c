#include "../defs.h"

extern volatile char keys[128];

char iKeys[128] = {0};
char iKeysOld[128] = {0};

void cguiInputUpdate() {

  int i;

  for (i = 0; i < 128; i++) {

    iKeysOld[i] = iKeys[i];
    iKeys[i] = keys[i];
    
  }
  
}

int cguiGetKey(int key) {

  return iKeys[key];
  
}

int cguiGetKeyDown(int key) {

  return (iKeys[key] && !iKeysOld[key]);
  
}

int cguiGetKeyUp(int key) {

  return (!iKeys[key] && iKeysOld[key]);
  
}


