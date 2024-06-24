#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


char*
fmtname(char *path)
{
	char *p;

	// 找到路径中最后一个斜杠的位置
	for(p=path+strlen(path); p >= path && *p != '/'; p--)
		;
	p++;	// 将 p 向前移动一位，使其指向斜杠之后的第一个字符，即文件名的开始
	return p;
}

void find(char *path, char *target) {
	
	//printf("path %s\n", path);
	//printf("target %s\n", target);
	char buf[512], *p;
	int fd;
	struct dirent de;
    struct stat st;
  
	if((fd = open(path, 0)) < 0){
		fprintf(2, "ls: cannot open %s\n", path);
		return;
	}
	
	if(fstat(fd, &st) < 0){
		fprintf(2, "ls: cannot stat %s\n", path);
		close(fd);
		return;
	}
	switch(st.type){
		case T_FILE:
			if(strcmp(fmtname(path), target) == 0)
				printf("%s\n", path);
		break;
		
		case T_DIR:			
			strcpy(buf, path);
			p = buf+strlen(buf);
			*p++ = '/';
			//printf("DIR NAME %s\n", buf);
			while(read(fd, &de, sizeof(de)) == sizeof(de)){
				if(de.inum == 0)
					continue;
				memmove(p, de.name, sizeof(de.name));
				if(stat(buf, &st) < 0){
					printf("ls: cannot stat %s\n", buf);
					continue;
				}
				if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
					continue;
				find(buf, target);
			}
			break;
	}
	close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("params error %d\n", argc);
    exit(0);
  }
  find(argv[1], argv[2]);
  exit(0);
}