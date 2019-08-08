CC=gcc
CLFAGS=-Wextra -Wall -pedantic

pty: mypty3.o mod2.o ansidec.o
	$(CC) -o pty mypty3.o mod2.o ansidec.o /usr/lib/x86_64-linux-gnu/libpulse.so

rec: rec_test.o kissfft/kiss_fftr.o kissfft/kiss_fft.o
	$(CC) -o rec rec_test.o kissfft/kiss_fftr.o kissfft/kiss_fft.o /usr/lib/x86_64-linux-gnu/libpulse-simple.so -lm

flips: flips.o
	$(CC) $(CFLAGS) -o flips flips.o /usr/lib/x86_64-linux-gnu/libpulse.so

term:
	tic 3stat.term
