#include <stdio.h>

int main(int argc, char **argv) {

  FILE *f;

  unsigned char header[18];
  unsigned char bmpPal[1024];

  unsigned int dibSize;

  int i;

  if (argc < 2) {

    printf("** [PAL2C] OPENING PALETTE.BMP...!  **\n");

    return 1;
  }

  f = fopen(argv[1], "rb");

  if (!f) {

    printf("** [ERROR] COULD NOT LOAD %s **\n", argv[1]);

    return 0;

  }

  /* PARSE BMP HEADER */

  fread(header, 1, 18, f);
  dibSize = header[14] | (header[15] << 8) | (header[16] << 16) |
              (header[17] << 24);
  fseek(f, 14 + dibSize, SEEK_SET);
  fread(bmpPal, 1, 1024, f);
  fclose(f);

  printf("/* PALETTE ARRAY (generated with Pal2c) */\n");
  printf("const unsigned char cgui_palette[768] = {\n      ");

  for (i = 0; i < 256; i++) {

    int r = bmpPal[(i * 4) + 2] >> 2;
    int g = bmpPal[(i * 4) + 1] >> 2;
    int b = bmpPal[(i * 4) + 0] >> 2;

    printf("%3d, %3d, %3d", r, g, b);

    if (i < 255)
      printf(", ");

    if ((i + 1) % 4 == 0 && i < 255)
      printf("\n    ");

  }

    printf("\n};\n");

    return 0;

}
