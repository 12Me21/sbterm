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
char *orig_prompt;
// Restore state when exiting
void exit_pty(int err) {
	setenv("TERM",orig_term,1);
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_settings);
	modem_end();
	exit(err);
}

char input[150];
int try_read(fd_set *fdset, int fd_read) {
	if(FD_ISSET(fd_read, fdset))
		return read(fd_read, input, sizeof input);
	return 0;
}		

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
	
	// Set RAW mode on stdin
	tcgetattr(STDIN_FILENO, &orig_term_settings);
	struct termios new_term_settings = orig_term_settings;
	cfmakeraw(&new_term_settings);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);
	
	// Create the child process
	if (fork()) {
		fd_set fd_in;

		// FATHER

		modem_init();

		struct timeval timeout = {0, 1};

		char ign=0;

		int inesc=0;
		
		while (1) {
			// Wait for data from standard input and master side of PTY
			FD_ZERO(&fd_in);
			FD_SET(0, &fd_in);
			FD_SET(master_fd, &fd_in);

			if (select(master_fd + 1, &fd_in, NULL, NULL, &timeout) == -1) {
				fprintf(stderr, "Error %d on select()\n", errno);
				exit_pty(1);
			}
			
			// If data on standard input
			int bytes=try_read(&fd_in, STDIN_FILENO);
			if(bytes>0){
				//if(memchr(input,3,sizeof input))
				//ign=1;
				write(master_fd,input,bytes);
			}else if(bytes<0){
				fprintf(stderr, "Error %d on read standard input\n", errno);
				exit_pty(1);
			}
			
			// If data on master side of PTY
			bytes=try_read(&fd_in, master_fd);
			if(bytes<sizeof input)
				ign=0;
			if(bytes>0){
				if(!ign){
					//write(STDOUT_FILENO,input,bytes);
					int i;
					for(i=0;i<bytes;i++){
						if(inesc){
							inesc=!proc_esc(input[i],modem_send);
						}else if(input[i]=='\033'){
							inesc=1;
							init_esc();
						}else{
							modem_send(input+i,1);
						}
					}
				}
			}else if(bytes<0){
				if(errno != EIO) //when child ends, exit cleanly
					fprintf(stderr, "Error %d on read master PTY\n", errno);
				exit_pty(1);
			}else{
				ign=0;
				modem_send("\026",1);
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
		//putenv("PS1=\\[\033[1;32m\\]\\u\\[\033[0m\\]:\\[\033[1;34m\\]\\w\\[\033[0m\\]\\$ ");
		// Execution of the program
		rc = execvp(argv[1], argv+1);

		// if Error...
		return 1;
	}

	return 0;
} // main
