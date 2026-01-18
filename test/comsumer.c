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
#define __NR_shmat 77

#define SIZE 50

_syscall2(int,sem_open,char*,name,int,value)

_syscall1(int,sem_wait,int, sem_n)

_syscall1(int,sem_post,int,sem_n)

_syscall1(int,sem_remove,char*, name)

