/*
 *  linux/kernel/sys.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <string.h>

int sys_ftime()
{
	return -ENOSYS;
}

int sys_break()
{
	return -ENOSYS;
}

int sys_ptrace()
{
	return -ENOSYS;
}

int sys_stty()
{
	return -ENOSYS;
}

int sys_gtty()
{
	return -ENOSYS;
}

int sys_rename()
{
	return -ENOSYS;
}

int sys_prof()
{
	return -ENOSYS;
}

int sys_setregid(int rgid, int egid)
{
	if (rgid>0) {
		if ((current->gid == rgid) || 
		    suser())
			current->gid = rgid;
		else
			return(-EPERM);
	}
	if (egid>0) {
		if ((current->gid == egid) ||
		    (current->egid == egid) ||
		    (current->sgid == egid) ||
		    suser())
			current->egid = egid;
		else
			return(-EPERM);
	}
	return 0;
}

int sys_setgid(int gid)
{
	return(sys_setregid(gid, gid));
}

int sys_acct()
{
	return -ENOSYS;
}

int sys_phys()
{
	return -ENOSYS;
}

int sys_lock()
{
	return -ENOSYS;
}

int sys_mpx()
{
	return -ENOSYS;
}

int sys_ulimit()
{
	return -ENOSYS;
}

int sys_time(long * tloc)
{
	int i;

	i = CURRENT_TIME;
	if (tloc) {
		verify_area(tloc,4);
		put_fs_long(i,(unsigned long *)tloc);
	}
	return i;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa.
 */
int sys_setreuid(int ruid, int euid)
{
	int old_ruid = current->uid;
	
	if (ruid>0) {
		if ((current->euid==ruid) ||
                    (old_ruid == ruid) ||
		    suser())
			current->uid = ruid;
		else
			return(-EPERM);
	}
	if (euid>0) {
		if ((old_ruid == euid) ||
                    (current->euid == euid) ||
		    suser())
			current->euid = euid;
		else {
			current->uid = old_ruid;
			return(-EPERM);
		}
	}
	return 0;
}

int sys_setuid(int uid)
{
	return(sys_setreuid(uid, uid));
}

int sys_stime(long * tptr)
{
	if (!suser())
		return -EPERM;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;
	return 0;
}

int sys_times(struct tms * tbuf)
{
	if (tbuf) {
		verify_area(tbuf,sizeof *tbuf);
		put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
		put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
		put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
		put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	}
	return jiffies;
}

int sys_brk(unsigned long end_data_seg)
{
	if (end_data_seg >= current->end_code &&
	    end_data_seg < current->start_stack - 16384)
		current->brk = end_data_seg;
	return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
int sys_setpgid(int pid, int pgid)
{
	int i;

	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = current->pid;
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->pid==pid) {
			if (task[i]->leader)
				return -EPERM;
			if (task[i]->session != current->session)
				return -EPERM;
			task[i]->pgrp = pgid;
			return 0;
		}
	return -ESRCH;
}

int sys_getpgrp(void)
{
	return current->pgrp;
}

int sys_setsid(void)
{
	if (current->leader && !suser())
		return -EPERM;
	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;
	return current->pgrp;
}

int sys_uname(struct utsname * name)
{
	static struct utsname thisname = {
		"linux .0","nodename","release ","version ","machine "
	};
	int i;

	if (!name) return -ERROR;
	verify_area(name,sizeof *name);
	for(i=0;i<sizeof *name;i++)
		put_fs_byte(((char *) &thisname)[i],i+(char *) name);
	return 0;
}

int sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return (old);
}
#define sem_name_size 32
#define sem_table_size 64
typedef struct {
	struct task_struct **p;  //4B
	int value;//4B
	char name[sem_name_size]; // 4B
} semaphore;
semaphore* sem_table[sem_table_size] = {NULL};

strcpfs2kn(char * dest,const char * src,int len){
	int i = 0;
	char * source = src;
	while (i<len)
	{
		char c = get_fs_byte(source++);
		dest[i++]=c;
		if(c=='\0')break; 
	}
}
int sys_sem_open(char* name,int value){
	//该函数执行在内核态，使用内核代码数据段，因为指针存储的是用户空间的逻辑地址，所以要用特殊的函数访问才能拿到正确的数据
	//1.查找该名称对应信号量，如果没有，就创建信号量，如果没有空间了，就返回-1；
	cli();
	int i = 0;
	int slot = -1;
	char name_kn[sem_name_size];
	strcpfs2kn(name_kn,name,sem_name_size);
	while(i<sem_table_size){
		if (sem_table[i]) {
			if(strcmp(name_kn,sem_table[i]->name)) // 如果找打了，就返回标识
				break;
		}else  slot = i; // 该槽位是空的
		i++;
	}// 1找到了 2没找到 
	if(i<sem_table_size) {
		sti();
		return i;
	}  
	if(slot>=0) { // 没找到，有空闲槽位
		sem_table[slot] = malloc(12);
		strcpy(sem_table[slot]->name,name_kn);
		sem_table[i]->value = value;
		sem_table[i]->p = malloc(4); //这里要修改
		*(sem_table[i]->p) = NULL;		
	}
	sti();
	return slot;
}

int sys_sem_wait(int sem_n){
	cli();
	int res ;
	if((res = sem_n>=0&&sem_n<sem_table_size&&sem_table[sem_n])) {
		if(--sem_table[sem_n]->value < 0 )
			sleep_on(sem_table[sem_n]->p); 
	} 
	sti();
	return !res;
}

int sys_sem_post(int sem_n){
	cli();
	int res ;
	if((res = sem_n>=0&&sem_n<sem_table_size&&sem_table[sem_n])) {
		if(++sem_table[sem_n]->value <= 0 )
			wake_up(sem_table[sem_n]->p); 
	} 
	sti();
	return !res;
}

int sys_sem_remove(char * name){
	cli();
	int i = 0;
	int slot = -1;
	char name_kn[sem_name_size];
	strcpfs2kn(name_kn,name,sem_name_size);
	while(i<sem_table_size){
		if (sem_table[i]&&strcmp(name_kn,sem_table[i]->name)) // 如果找到了，就返回标识
			break;
		i++;
	}
	if(i<sem_table_size){ // 找到了
		semaphore * cur = sem_table[i];
		free_s(cur->p,4);
		free_s(cur->name,sem_name_size);
		free_s(cur,12);
	}
	sti();
	return 0;
}