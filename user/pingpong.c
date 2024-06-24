
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(int argc, char **argv) {

	int p1[2]; // 父->子
	pipe(p1);
	int p2[2]; // 
	pipe(p2);
	char buf[10];
	if(fork() == 0) {   // child
		read(p1[0], buf, 1);
		printf("%d: recveived ping\n", getpid());
		write(p2[1], buf, 1);
	} else {			// parent
		//
		write(p1[1], "1", 1);
		read(p2[0], buf, 1);
		printf("%d: recveived pong\n", getpid());
		wait(0);
	}
	exit(0);
}
