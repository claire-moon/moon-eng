#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "mdp/mdp-format.h"

#define TEST_W 64;
#define TEST_H 64;
#define TEST_COUNT (TEST_W * TEST_H);

/* GLOBAL VARS */

MapCell *currentCells = NULL;

int mapWidth = 0;
int mapHeight = 0;
int mapCellSize = MAP_DEFAULT_CELL_SIZE;
int activeSkybox = 1;

static void clearMapCells(MapCell *cells, int count) {

  int i, r;

  for (i = 0; i < count; i++) {

    cells[i].flags = MAP_CELL_SKY;

    cells[i].wallHeight = 0;
    cells[i].floorHeight = 0;
    cells[i].ceilingHeight = 0;

    cells[i].wallTex = 0;
    cells[i].floorTex = 0;
    cells[i].ceilingTex = 0;

    cells[i].light = 30;
    cells[i].tag = 0;

    for (r = 0; r < 6; r++) {

      cells[i].reserved[r] = 0;
      
    }
    
  }
  
}

static void setCellRect(MapCell *cells, int w, int h, int x, int y, int rw,
                        int rh, unsigned short flags, unsigned short wallHeight,
                        unsigned short floorHeight, unsigned char ceilingHeight,
                        unsigned char wallTex, unsigned char floorTex,
                        unsigned char ceilingTex, unsigned char light,
                        unsigned char tag) {

  int ix;
  int iy;
  int idx;

  for (iy = y; iy < y + rh; iy++) {

    for (ix = x; ix < x + rw; ix++) {

      if (ix >= 0 && ix < w && iy >= 0 && iy < h) {

        idx = (iy * w) + ix;

        cells[idx].flags = flags;
        cells[idx].wallHeight = wallHeight;
        cells[idx].floorHeight = floorHeight;
        cells[idx].ceilingHeight = ceilingHeight;
        cells[idx].wallTex = wallTex;
        cells[idx].floorTex = floorTex;
        cells[idx].ceilingTex = ceilingTex;
        cells[idx].light = light;
	cells[idx].tag = tag;
	
      }
      
    }
    
  }
  
}

MapCell *mapCellAt(int x, int y) {

  if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) {

    return NULL;
    
  }

  return &currentCells[(y * mapWidth) + x];
  
}

int mapCellBlocksPlayer(int x, int y) {

  MapCell *cell;

  cell = mapCellAt(x, y);

  if (!cell) {

    return 1;
    
  }

  if (cell->flags & MAP_CELL_COLLIDE) {

    return 1;
    
  }

  return 0;
  
}

int mapCellIsSolid(int x, int y) {

  MapCell *cell;

  cell = mapCellAt(x, y);

  if (!cell) {

    return 0;
    
  }

  return (cell->flags & MAP_CELL_SOLID) != 0;
  
}

int mapCellHasSky(int x, int y) {

  MapCell *cell;

  cell = mapCellAt(x, y);

  if (!cell) {

    return 1;
    
  }

  return (cell->flags & MAP_CELL_SKY) != 0;
  
}

int mapCellWallHeight(int x, int y) {

  MapCell *cell;

  cell = mapCellAt(x, y);

  if (!cell) {

    return 0;
    
  }

  return cell->wallHeight;
  
}

int mapCellCeilingHeight(int x, int y) {

  MapCell *cell;

  cell = mapCellAt(x, y);

  if (!cell) {

    return 0;
    
  }

  return cell->ceilingHeight;
  
}

int mapCellLight(int x, int y) {

  MapCell *cell;

  cell = mapCellAt(x, y);

  if (!cell) {

    return 30;
    
  }

  return cell->light;
  
}

/* BOOTSTRAPPER */

void initMapSystem() {

	FILE *check = fopen(mdpFilename, "rb");

	if (check) {

		fclose(check);

		return;

	}

	/* IF FILE MISSING ... */

        MapCell *tempCells;
        FILE *f;
        MoonHeader head;
        MoonEntry dir[1];
        LevelHeader lvlHead;
	int cellDataSize;

        tempCells = malloc(sizeof(MapCell) * TEST_COUNT);

        if (!tempCells) {

	  return;
	  
	}

	clearMapCells(tempCells, TEST_COUNT);
	
	fclose(f);

}

/* ARCHIVE PARSER */

void loadMap(int mapNum) {

	char mapName[16];
	char litName[16];
	FILE *f;
	MoonHeader head;
	MoonEntry entry;
	LevelHeader lvlHead;
	long returnPos;
	int i;

	sprintf(mapName, "MAP%02d", mapNum);
	sprintf(litName, "MAP%02dLIT", mapNum);

	f = fopen(mdpFilename, "rb");
	
	if (!f) return;

	fread(&head, sizeof(MoonHeader), 1, f);
	fseek(f, head.dirOffset, SEEK_SET);

	for (i = 0; i < head.numLumps; i++) {

	  fread(&entry, sizeof(MoonEntry), 1, f);
	  
	  returnPos = ftell(f);

	  if (strcmp(entry.name, mapName) == 0) {

	    fseek(f, entry.offset, SEEK_SET);
	    
	    /* READ LEVEL HEADER */
	    
	    fread(&lvlHead, sizeof(LevelHeader), 1, f);
	    
	    mapWidth      = lvlHead.width;
	    mapHeight     = lvlHead.height;
	    activeSkybox  = lvlHead.skyboxID;

	    /* ALLOCATE MEM NEEDED */

	    if (currentMap) free(currentMap);

	    currentMap = malloc(mapWidth * mapHeight * sizeof(int));

	    fread(currentMap, mapWidth * mapHeight * sizeof(int), 1, f);
	    
	  }

	  else if (strcmp(entry.name, litName) == 0) {

	    fseek(f, entry.offset, SEEK_SET);

	    if (currentLight) free(currentLight);

	    currentLight = malloc(mapWidth * mapHeight * sizeof(int));

	    fread(currentLight, entry.size, 1, f);
	    
	  }

	  fseek(f, returnPos, SEEK_SET);

	}

	fclose(f);

}


