//Based on mypty3.c from http://rachid.koucha.free.fr/tech_corner/pty_pdip.html

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define __USE_BSD
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "pty.h"

struct termios orig_term_settings; // Saved terminal settings
void exit_pty(int err){
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_settings);
	exit(err);
}

int main(int argc, char **argv, char **env){
	if(argc <= 1){
		fprintf(stderr, "Usage: %s program_name [parameters]\n", argv[0]);
		return 1;
	}

	int master_fd = posix_openpt(O_RDWR);
	grantpt(master_fd);
	unlockpt(master_fd);

	tcgetattr(STDIN_FILENO, &orig_term_settings);
	struct termios new_term_settings = orig_term_settings;
	cfmakeraw(&new_term_settings);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);

	// PARENT
	if(fork()){
		modem_init(master_fd);
		char input[150];

		while(1){
			int bytes = read(STDIN_FILENO, input, sizeof input);
			if(bytes > 0)
				write(master_fd, input, bytes);
			else if(bytes < 0){
				fprintf(stderr, "Error %d on reading stdin\n", errno);
				exit_pty(1);
			}
		}
	}
	// CHILD
	int slave_fd = open(ptsname(master_fd), O_RDWR);
	close(master_fd);

	// Set terminal size
	struct winsize winsize = {30, 50, 400, 240};
	ioctl(slave_fd, TIOCSWINSZ, &winsize);

	dup2(slave_fd, STDIN_FILENO);
	dup2(slave_fd, STDOUT_FILENO);
	dup2(slave_fd, STDERR_FILENO);
	close(slave_fd);

	setsid();
	putenv("TERM=xterm-sb");
	putenv("LANG=");

	execvp(argv[1], argv+1);

	return 0;
}
