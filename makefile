CC=gcc
CFLAGS=-Wextra -Wall -pedantic -Wno-sign-compare -Wno-unused-parameter

pty: main.o mod4.o ansidec.o pulse.o
	$(CC) $(CFLAGS) -o pty main.o mod4.o ansidec.o pulse.o /usr/lib/x86_64-linux-gnu/libpulse.so

term:
	tic 3stat.term
