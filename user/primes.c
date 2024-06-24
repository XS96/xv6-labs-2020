#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h" 

void filter(int leftP[2]) {
	int n;
	read(leftP[0], &n, sizeof(n));
	if(n == -1) {
		printf("pid%d exit n = %d\n", getpid(), n);
		exit(0);
	}
	printf("pid%d: prime %d\n", getpid(), n);
	
	int rightP[2];
	pipe(rightP);
	
	if(fork() != 0) {
		close(rightP[0]);
		int buf;
		while(read(leftP[0], &buf, sizeof(buf)) && buf != -1) {
			if(buf % n != 0) {
				write(rightP[1], &buf, sizeof(buf));
			}
		}
		buf = -1;
		write(rightP[1], &buf, sizeof(buf));
		wait(0);
	} else {
		close(rightP[1]);
		close(leftP[0]);
		filter(rightP);
	}
	exit(0);
}


int main(int argc, char **argv) {
	/*
		p1：循环2到35 看是否整除2
			若有不整除的，创建一个新进程，将其发送至新进程
	    pi: 
			读取从父进程发来的数字，打印第一个数n
			看发来的后面的数是否能被n整除
			若有不整除的，创建一个新进程，将其发送至新进程
	*/
	int input_pipe[2];
	pipe(input_pipe);
	
	if(fork() != 0) {
		close(input_pipe[0]);
		for(int i = 2; i <= 35; i++) {
			write(input_pipe[1], &i, sizeof(i));
		}
		int i = -1;
		write(input_pipe[1], &i, sizeof(i));
		wait(0);
	} else {
		close(input_pipe[1]);
		filter(input_pipe);
		exit(0);
	}
	exit(0);
}


