CC=gcc

pty: mypty3.o mod.o
	$(CC) -o pty mypty3.o mod.o /usr/lib/x86_64-linux-gnu/libpulse*

mod: mod.o
	$(CC) -o mod mod.o /usr/lib/x86_64-linux-gnu/libpulse*
