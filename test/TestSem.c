#define __LIBRARY__

#include <stdio.h>

#include <unistd.h>

#include <string.h>

#include <fcntl.h>  

#define __NR_sem_open	72

#define __NR_sem_wait	73

#define __NR_sem_post	74

#define __NR_sem_remove	75

#define SIZE 50

_syscall2(int,sem_open,char*,name,int,value)

_syscall1(int,sem_wait,int, sem_n)

_syscall1(int,sem_post,int,sem_n)

_syscall1(int,sem_remove,char*, name)



int main(int argc, char *argv[]){

    int ttyfd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int empty = sem_open("empty",SIZE);
    int full = sem_open("full",0);
    int mutex = sem_open("mutex",1);

        if (!fork()) {
            short cnt_ch = 1;
            int buffd = open("buffer.txt", O_RDWR | O_CREAT, 0644);
            short readitem;
            char buffer[10];
            while (cnt_ch<=10) {
                sem_wait(full); 
                read(buffd,(char*)&readitem,2); 
                sprintf(buffer, "c %d\n", readitem); //log_1 
                sem_wait(mutex);
                write(ttyfd, buffer, strlen(buffer)); //log_2
                sem_post(mutex);
                sem_post(empty);
                cnt_ch++;
            }
            return 0;
        }
		short cnt_p = 1;

		char str[10];

	    int buffd2 = open("buffer.txt", O_RDWR | O_CREAT , 0644);

	    while (cnt_p<=SIZE) {

			sem_wait(empty);
      
			write(buffd2,(char*)&cnt_p,2);
			sprintf(str,"p %d\n",cnt_p);

			sem_wait(mutex);
			write(ttyfd,str,strlen(str));
			sem_post(mutex);    

			sem_post(full);

			cnt_p++;

	    }
	return 0;

}