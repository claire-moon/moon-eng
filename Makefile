# MOON ENG
# MAKEFILE
# CG MOON 2026

# --- OS DETECTION ---

ifdef COMSPEC

CC = C:\djgpp\bin\gcc.exe
RM = del

else

CC = i586-pc-msdosdjgpp-gcc
RM = rm -f

endif

# ------------------

CFLAGS = -O2 -Wall
LIBS   = -lm

# --------------------

ZEUS_OUT  = zeus.exe
ZEUS_OBJS = main.o map.o input.o render.o console.o player.o physics.o hud.o skybox.o sprite.o

MDPED_OUT = mdped.exe
MDPED_SRC = mdped/mdped.c cgui/cgui-main.c cgui/cgui-widgets.c cgui/cgui-font.c

TMUSE_OUT = tmuse.exe
TMUSE_SRC = tmuse/tmuse-main.c tmuse/tmuse-tui.c tmuse/tmuse-dsp.c tmuse/tmuse-mix.c tmuse/tmuse-io.c tmuse/tmuse-music.c tmuse/tmuse-dash.c cgui/cgui-input.c input.c

TMUSE_GUI_OUT = tmusegui.exe
TMUSE_GUI_SRC = tmuse/tmuse-gui.c tmuse/tmuse-main.c tmuse/tmuse-dsp.c tmuse/tmuse-mix.c tmuse/tmuse-io.c tmuse/tmuse-music.c tmuse/tmuse-dash.c cgui/cgui-main.c cgui/cgui-widgets.c cgui/cgui-input.c cgui/cgui-font.c input.c

all: $(ZEUS_OUT) $(MDPED_OUT) $(TMUSE_OUT) $(TMUSE_GUI_OUT)

$(ZEUS_OUT): $(ZEUS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(MDPED_OUT): $(MDPED_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TMUSE_OUT): $(TMUSE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(TMUSE_GUI_OUT): $(TMUSE_GUI_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o
	$(RM) $(ZEUS_OUT)
	$(RM) $(MDPED_OUT)
	$(RM) $(TMUSE_OUT)
