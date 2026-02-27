#include <windows.h>
#include <stdio.h>
#include <string.h>

#define MAP_SIZE 8
#define CELL_SIZE 40

int mapData [MAP_SIZE][MAP_SIZE];
int mapOffset = 0;

/*  ARCHIVAL STRUCTURES */

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

/* FILE I/O */

void LoadMap() {

	FILE *f = fopen("C:/zeus/game.moon", "rb");

	if (!f) {

		MessageBox(NULL, "game.moon not found! Run the engine to generate!", "Error", MB_OK);
		return;

	}

	MoonHeader head;
	fread(&head, sizeof(MoonHeader), 1, f);
	fseek(f, head.dirOffset, SEEK_SET);

	for (int i = 0; i < head.numLumps; i++) {

		MoonEntry entry;
		fread(&entry, sizeof(MoonEntry), 1, f);
		
		if (strcmp(entry.name, "MAP01") == 0) {

			mapOffset = entry.offset;
			fseek(f, entry.offset, SEEK_SET);
			fread(mapData, entry.size, 1, f);
			break;

		}

	}

	fclose(f);

}

void SaveMap() {

	if (mapOffset == 0) return;
	
	FILE *f = fopen("C:/zeus/game.moon", "r+b");

		if (f) {

			fseek(f, mapOffset, SEEK_SET);
			fwrite(mapData, sizeof(mapData), 1, f);
			fclose(f);

		}

}

/* --- WIN32 WINDOW PROCEDURE --- */

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch(msg) {

	case WM_CREATE:

		LoadMap();
		break;

	case WM_PAINT: {

		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		for (int y = 0; y < MAP_SIZE; y++) {


			for (int x = 0; x < MAP_SIZE; x++) {

				RECT r = {x * CELL_SIZE, y * CELL_SIZE, (x + 1) * CELL_SIZE, (y + 1) * CELL_SIZE};
				HBRUSH brush = (HBRUSH)GetStockObject(BLACK_BRUSH);

				if (mapData[y][x] == 1) brush = CreateSolidBrush(RGB(255, 128, 0));
				else if (mapData[y][x] == 2) brush = CreateSolidBrush(RGB(0, 0, 255));
				else if (mapData[y][x] == 3) brush = CreateSolidBrush(RGB(255, 0, 0));

				FillRect(hdc, &r, brush);

				if (mapData[y][x] != 0) DeleteObject(brush);

				/* DRAW GRID BOUNDS */

				FrameRect(hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));

			}

		}

		EndPaint(hwnd, &ps);
		break;

	}

	case WM_LBUTTONDOWN: {

		int x = LOWORD(lParam) / CELL_SIZE;
		int y = HIWORD(lParam) / CELL_SIZE;

		if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {

			mapData[y][x]++;

			if (mapData[y][x] > 3) mapData[y][x] = 0;

			InvalidateRect(hwnd, NULL, TRUE);


		}

		break;

	}

	case WM_DESTROY:

		SaveMap();
		PostQuitMessage(0);
		break;

	default: return DefWindowProc(hwnd, msg, wParam, lParam);

	}

	return 0;

}

/* --- WIN32 ENTRY POINT --- */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {

	WNDCLASS wc = {0};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "MOONEDIT";
	RegisterClass(&wc);

	int winWidth  = (MAP_SIZE * CELL_SIZE) + 16;
	int winHeight = (MAP_SIZE * CELL_SIZE) + 39;

	HWND hwnd = CreateWindow("MOONEDIT", "Moon Engine Editor",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
		100, 100, winWidth, winHeight, NULL, NULL, hInst, NULL);

	MSG msg;

	while(GetMessage(&msg, NULL, 0, 0)) {

		TranslateMessage(&msg);
		DispatchMessage(&msg);

	}

	return 0;


}