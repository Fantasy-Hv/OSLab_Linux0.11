#define __LIBRARY__



#include <stdio.h>

#include <unistd.h>

#include <string.h>

#include <fcntl.h>  



#define __NR_sem_open	72

#define __NR_sem_wait	73

#define __NR_sem_post	74

#define __NR_sem_remove	75

#define __NR_shmget 76

#define __NR_shmdt 77



#define ITEM_NUM 15
#define BUFFER_SIZE 5
_syscall2(int,sem_open,char*,name,int,value)

_syscall1(int,sem_wait,int, sem_n)

_syscall1(int,sem_post,int,sem_n)

_syscall1(int,sem_remove,char*, name)

_syscall2(unsigned long, shmget, int , key, size_t, size)

int main(int argc, char *argv[]){
    int empty = sem_open("empty",BUFFER_SIZE);
    int full = sem_open("full",0);
    int mutex = sem_open("mutex",1);
    char * shm = (char*)shmget(4,BUFFER_SIZE);
    int i = 0,n = 0;
    char str[10];
    
    while (n< ITEM_NUM) {
        sem_wait(empty);
        shm[i]=n;
        sem_wait(mutex);
        printf("p%d ",n);
        if(i==BUFFER_SIZE-1)printf("\n");
        sem_post(mutex);
        sem_post(full);
        i= (i+1)%BUFFER_SIZE;
	    n++;
    }
    
    return 0;
}