#ifndef MDP_FORMAT_H
#define MDP_FORMAT_H

#define MAP_FORMAT_VERSION 1
#define MAP_DEFAULT_CELL_SIZE 64

#define MAP_CELL_SOLID 0x0001
#define MAP_CELL_SKY 0x0002
#define MAP_CELL_COLLIDE 0x0004
#define MAP_CELL_DOOR 0x0008
#define MAP_CELL_TRIGGER 0x0010
#define MAP_CELL_SECRET 0x0020
#define MAP_CELL_DAMAGE 0x0040
#define MAP_CELL_TRANS 0x0080

typedef unsigned char mdp_u8;
typedef unsigned char mdp_u16;

typedef struct {

  char magic[4];
  int numLumps;
  int dirOffset;
  
} MoonHeader;

typedef struct {

  int offset;
  int size;
  char name[16];
  
} MoonEntry;

typedef struct {

  int width;
  int height;
  int cellSize;
  int skyboxID;
  int formatVersion;
  
} LevelHeader;

typedef struct {

  mdp_u16 flags;

  mdp_u8 wallHeight;
  mdp_u8 floorHeight;
  mdp_u8 ceilingHeight;

  mdp_u8 wallTex;
  mdp_u8 floorTex;
  mdp_u8 ceilingTex;

  mdp_u8 light;
  mdp_u8 tag;

  mdp_u8 reserved[6];
  
} MapCell;

#endif
