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

# --------------------

OBJS = main.o map.o input.o render.o console.o player.o physics.o hud.o skybox.o sprite.o

all: zeus.exe mdped.exe

zeus.exe: $(OBJS)
	$(CC) -o zeus.exe $(OBJS) -lm

mdped.exe: mdped.c
	$(CC) -o mdped.exe mdped.c

%.o: %.c
	$(CC) -c $< -o $@

clean:
	$(RM) *.o
	$(RM) zeus.exe
	$(RM) mdped.exe
