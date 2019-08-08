#define _XOPEN_SOURCE 600
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#define __USE_BSD
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "pty.h"

struct termios orig_term_settings; // Saved terminal settings
char *orig_term; //value of TERM
char *orig_lang;
// Restore state when exiting
void exit_pty(int err) {
	setenv("TERM",orig_term,1);
	setenv("LANG",orig_lang,1);
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_settings);
	modem_end();
	exit(err);
}

char input[150];

int main(int argc, char *argv[]) {
	int rc;

	// Check arguments
	if (argc <= 1) {
		fprintf(stderr, "Usage: %s program_name [parameters]\n", argv[0]);
		return 1;
	}
	
	int master_fd = posix_openpt(O_RDWR);
	if (master_fd < 0) {
		fprintf(stderr, "Error %d on posix_openpt()\n", errno);
		return 1;
	}
	if (grantpt(master_fd)) {
		fprintf(stderr, "Error %d on grantpt()\n", errno);
		return 1;
	}
	if (unlockpt(master_fd)) {
		fprintf(stderr, "Error %d on unlockpt()\n", errno);
		return 1;
	}

	orig_term=getenv("TERM");
	orig_lang=getenv("LANG");
	
	// Set RAW mode on stdin
	tcgetattr(STDIN_FILENO, &orig_term_settings);
	struct termios new_term_settings = orig_term_settings;
	cfmakeraw(&new_term_settings);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);
	
	// Create the child process
	if (fork()) {

		// FATHER

		modem_init(master_fd);

		//struct pollfd pollfds[1] = {.fd = STDIN_FILENO, .events = POLLIN};
		
		while (1) {
			// If data on standard input
			int bytes=read(STDIN_FILENO, input, sizeof input);
			printf("read returne: %d:",bytes);
			write(STDOUT_FILENO,input,bytes);
			
			if(bytes>0){
				write(master_fd,input,bytes);
			}else if(bytes<0){
				fprintf(stderr, "Error %d on read standard input\n", errno);
				exit_pty(1);
			}
		} // End while
		
	} else {
		// CHILD

		// Open the slave side ot the PTY
		int slave_fd = open(ptsname(master_fd), O_RDWR);
		
		// Close the master side of the PTY
		close(master_fd);

		// Set terminal size
		struct winsize winsize = {30, 50, 400, 240};
		ioctl(slave_fd, TIOCSWINSZ, &winsize);
		
		// The slave side of the PTY becomes the standard input and outputs of the child process
		dup2(slave_fd,STDIN_FILENO);
		dup2(slave_fd,STDOUT_FILENO);
		dup2(slave_fd,STDERR_FILENO);

		//FILE *f = fdopen(STDOUT_FILENO, "w");
		//setvbuf(f, NULL, _IOFBF, 50);
		
		struct termios stdio_settings;
		tcgetattr(STDOUT_FILENO, &stdio_settings);
		cfsetospeed(&stdio_settings, B300);
		tcsetattr(STDOUT_FILENO, TCSANOW, &stdio_settings);
		
		// Now the original file descriptor is useless
		close(slave_fd);

		// Make the current process a new session leader
		setsid();

		// As the child is a session leader, set the controlling terminal to be the slave side of the PTY
		// (Mandatory for programs like the shell to make them manage correctly their outputs)
		ioctl(STDIN_FILENO, TIOCSCTTY, 1);

		putenv("TERM=xterm-sb");
		putenv("LANG=");
		//putenv("PS1=\\[\033[1;32m\\]\\u\\[\033[0m\\]:\\[\033[1;34m\\]\\w\\[\033[0m\\]\\$ ");
		// Execution of the program
		rc = execvp(argv[1], argv+1);

		// if Error...
		return 1;
	}

	return 0;
} // main
