#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <sys/signalfd.h>
#include <poll.h>
#define LENGTH(X) (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH		50
/* Room for every block at its full length, so getstatus() cannot run off the
 * end no matter how much the commands print or how many blocks are enabled. */
#define STATUSLENGTH		(LENGTH(blocks) * CMDLENGTH + 1)

typedef struct {
	char* icon;
	char* command;
	unsigned int interval;
	unsigned int signal;
} Block;
void sighandler();
void buttonhandler(int ssi_int);
void replace(char *str, char old, char new);
void remove_all(char *str, char to_remove);
void getcmds(int time);
void getsigcmds(int signal);
void setupsignals();
int getstatus(char *str, char *last);
void setroot();
void statusloop();
void termhandler(int signum);


#include "config.h"

static Display *dpy;
static int screen;
static Window root;
static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;
static int signalFD;
static int timerInterval = -1;
static void (*writestatus) () = setroot;

void replace(char *str, char old, char new)
{
	for(char * c = str; *c; c++)
		if(*c == old)
			*c = new;
}

// the previous function looked nice but unfortunately it didnt work if to_remove was in any position other than the last character
// theres probably still a better way of doing this
void remove_all(char *str, char to_remove) {
	char *read = str;
	char *write = str;
	while (*read) {
		if (*read != to_remove) {
			*write++ = *read;
		}
		++read;
	}
	*write = '\0';
}

int gcd(int a, int b)
{
	int temp;
	while (b > 0){
		temp = a % b;

		a = b;
		b = temp;
	}
	return a;
}


//opens process *cmd and stores output in *output
void getcmd(const Block *block, char *output)
{
	if (block->signal)
	{
		output[0] = block->signal;
		output++;
	}
	char *cmd = block->command;
	FILE *cmdf = popen(cmd,"r");
	if (!cmdf){
        //printf("failed to run: %s, %d\n", block->command, errno);
		return;
    }
    char tmpstr[CMDLENGTH] = "";
    // TODO decide whether its better to use the last value till next time or just keep trying while the error was the interrupt
    // this keeps trying to read if it got nothing and the error was an interrupt
    //  could also just read to a separate buffer and not move the data over if interrupted
    //  this way will take longer trying to complete 1 thing but will get it done
    //  the other way will move on to keep going with everything and the part that failed to read will be wrong till its updated again
    // either way you have to save the data to a temp buffer because when it fails it writes nothing and then then it gets displayed before this finishes
	char * s;
    int e;
    do {
        errno = 0;
        s = fgets(tmpstr, CMDLENGTH-(strlen(delim)+1), cmdf);
        e = errno;
        // Stop retrying once a termination signal has been handled, otherwise
        // a block that never produces output makes the process unkillable.
    } while (!s && e == EINTR && statusContinue);
	pclose(cmdf);
	// output points into a CMDLENGTH slot, one byte in when a signal byte was
	// written. The old code assumed an empty icon: with any icon set, copying
	// icon+value+delim ran past the slot into the next block's.
	size_t avail = CMDLENGTH - (block->signal ? 1 : 0);
	snprintf(output, avail, "%s%s", block->icon, tmpstr);
	remove_all(output, '\n');
	size_t i = strlen(output);
    if ((i > 0 && block != &blocks[LENGTH(blocks) - 1])){
        strncat(output, delim, avail - i - 1);
    }
}

void getcmds(int time)
{
	const Block* current;
	for(int i = 0; i < LENGTH(blocks); i++)
	{
		current = blocks + i;
		if ((current->interval != 0 && time % current->interval == 0) || time == -1){
			getcmd(current,statusbar[i]);
        }
	}
}

void getsigcmds(int signal)
{
	const Block *current;
	for (int i = 0; i < LENGTH(blocks); i++)
	{
		current = blocks + i;
		if (current->signal == signal){
			getcmd(current,statusbar[i]);
        }
	}
}

void setupsignals()
{
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGALRM); // Timer events
	sigaddset(&signals, SIGUSR1); // Button events
	// All signals assigned to blocks
	for (size_t i = 0; i < LENGTH(blocks); i++)
		if (blocks[i].signal > 0)
			sigaddset(&signals, SIGRTMIN + blocks[i].signal);
	// Create signal file descriptor for pooling
	signalFD = signalfd(-1, &signals, 0);
	// Block all real-time signals
	for (int i = SIGRTMIN; i <= SIGRTMAX; i++) sigaddset(&signals, i);
	sigprocmask(SIG_BLOCK, &signals, NULL);
	// Do not transform children into zombies
	struct sigaction sigchld_action = {
  		.sa_handler = SIG_DFL,
  		.sa_flags = SA_NOCLDWAIT
	};
	sigaction(SIGCHLD, &sigchld_action, NULL);

}

int getstatus(char *str, char *last)
{
	size_t len;

	strcpy(last, str);
	str[0] = '\0';
    for(int i = 0; i < LENGTH(blocks); i++) {
		// Bounded: str holds STATUSLENGTH bytes. Unbounded strcat here used to
		// be sized for a 256-byte buffer that the blocks could outgrow.
		len = strlen(str);
		strncat(str, statusbar[i], STATUSLENGTH - len - 1);
        if (i == LENGTH(blocks) - 1) {
			len = strlen(str);
			strncat(str, " ", STATUSLENGTH - len - 1);
		}
    }
	if ((len = strlen(str)) > 0)
		str[len-1] = '\0';
	return strcmp(str, last);//0 if they are the same
}

void setroot()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only set root if text has changed.
		return;
	// The connection is opened once in main() and kept for the life of the
	// process. Reconnecting on every update meant a full X handshake every
	// time any block ticked, and left dpy dangling whenever XOpenDisplay
	// failed, since the previous display had already been closed.
	XStoreName(dpy, root, statusstr[0]);
	XFlush(dpy);
}

void pstdout()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only write out if text has changed.
		return;
	printf("%s\n",statusstr[0]);
	fflush(stdout);
}


void statusloop()
{
	setupsignals();
    // first figure out the default wait interval by finding the
    // greatest common denominator of the intervals
    for(int i = 0; i < LENGTH(blocks); i++){
        if(blocks[i].interval){
            timerInterval = gcd(blocks[i].interval, timerInterval);
        }
    }
    getcmds(-1);     // Fist time run all commands
    if (signalFD < 0) {
        fprintf(stderr, "dwmblocks: cannot create signal file descriptor: %s\n",
                strerror(errno));
        return;
    }
    raise(SIGALRM);  // Schedule first timer event
    int ret;
    struct pollfd pfd[] = {{.fd = signalFD, .events = POLLIN}};
    while (statusContinue) {
        // Wait for new signal
        ret = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), -1);
        if (ret < 0) {
            // An interrupted wait is not a reason to stop the status bar.
            if (errno == EINTR)
                continue;
            fprintf(stderr, "dwmblocks: poll failed: %s\n", strerror(errno));
            break;
        }
        if (pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
        if (!(pfd[0].revents & POLLIN))
            continue;
        sighandler(); // Handle signal
    }
}

void sighandler()
{
	static int time = 0;
	struct signalfd_siginfo si;
	int ret = read(signalFD, &si, sizeof(si));
	if (ret < 0) return;
	int signal = si.ssi_signo;
	switch (signal) {
		case SIGALRM:
			// Execute blocks and schedule the next timer event
			getcmds(time);
			alarm(timerInterval);
			time += timerInterval;
			break;
		case SIGUSR1:
			// Handle buttons
			buttonhandler(si.ssi_int);
			return;
		default:
			// Execute the block that has the given signal
			getsigcmds(signal - SIGRTMIN);
			break;
	}
	writestatus();
}

void buttonhandler(int ssi_int)
{
	// '+' binds tighter than '&', so this was ('0' + ssi_int) & 0xff, which
	// only happened to give the right digit for the button numbers in use.
	char button[2] = {'0' + (ssi_int & 0xff), '\0'};
	pid_t process_id = getpid();
	int sig = ssi_int >> 8;
	if (fork() == 0)
	{
		const Block *current = NULL;
		for (int i = 0; i < LENGTH(blocks); i++)
		{
			if (blocks[i].signal == sig) {
				current = blocks + i;
				break;
			}
		}
		// A signal with no matching block used to leave current pointing at
		// the last block, and the click ran that block's command instead.
		if (!current)
			exit(EXIT_SUCCESS);
		char shcmd[1024];
		snprintf(shcmd,sizeof shcmd,"%s && kill -%d %d",current->command, current->signal+34,process_id);
		char *command[] = { "/bin/sh", "-c", shcmd, NULL };
		setenv("BLOCK_BUTTON", button, 1);
		setsid();
		execvp(command[0], command);
		exit(EXIT_SUCCESS);
	}
}


void termhandler(int signum)
{
	statusContinue = 0;
}

int main(int argc, char** argv)
{
	for(int i = 0; i < argc; i++)
	{
		if (!strcmp("-d",argv[i]))
			delim = argv[++i];
		else if(!strcmp("-p",argv[i]))
			writestatus = pstdout;
	}
	// Only the root-window writer needs X; -p just prints to stdout.
	if (writestatus == setroot) {
		if (!(dpy = XOpenDisplay(NULL))) {
			fprintf(stderr, "dwmblocks: cannot open display\n");
			return 1;
		}
		screen = DefaultScreen(dpy);
		root = RootWindow(dpy, screen);
	}
	signal(SIGTERM, termhandler);
	signal(SIGINT, termhandler);
	statusloop();
	close(signalFD);
	if (dpy)
		XCloseDisplay(dpy);
	return 0;
}
