CC=gcc

pty: mypty3.o mod.o
	$(CC) -o pty mypty3.o mod.o /usr/lib/x86_64-linux-gnu/libpulse-simple.so

term: /etc/terminfo/x/xterm-sb
	sudo tic xtermsb.term
