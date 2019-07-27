CC=gcc

pty: mypty3.o mod.o ansidec.o
	$(CC) -o pty mypty3.o mod.o ansidec.o /usr/lib/x86_64-linux-gnu/libpulse-simple.so

term:
	tic 3stat.term
