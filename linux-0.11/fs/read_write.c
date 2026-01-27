/*
 *  linux/fs/read_write.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/segment.h>

extern int rw_char(int rw,int dev, char * buf, int count, off_t * pos);
extern int read_pipe(struct m_inode * inode, char * buf, int count);
extern int write_pipe(struct m_inode * inode, char * buf, int count);
extern int block_read(int dev, off_t * pos, char * buf, int count);
extern int block_write(int dev, off_t * pos, char * buf, int count);
extern int file_read(struct m_inode * inode, struct file * filp,
		char * buf, int count);
extern int file_write(struct m_inode * inode, struct file * filp,
		char * buf, int count);

#define show_entry_len 41 // 5*4（数字）+4*5（空格）+1（\n）
char table_head[] = "pid    father    stat    counter    start_time\n";//49bytes,remember to remove \0 when use
//根据pos指定的位置 ，将内容写到缓冲区中，并修改pos，返回读取的字节数
int proc_read(unsigned short dev,char* buf,int count,off_t* pos){
	// 输出所有进程的pid，state，等信息，可以参考sched.c的show_task
	// 难点在于根据pos找到对应的输出位置,pos是已经读了多少字节
	// 1.输出表头行，
	// 2.输出进程格式化行
	/**
	 * long state, 4
	 * long counter, 4
	 * long pid, 4
	 * long father, 4
	 * long start_time 4
	 */
	int wn = 0; //已经写入缓冲区的字节数,可以用来指示buff中下一个空位
	if(*pos<48){ //如果还没打完表头
		wn = strcp(buf,table_head+*pos,48,count, KNFS);
		*pos+= wn;
	}
// 1.pos到输入字符串的映射，pos到进程号的映射
	int i; // 需要根据pos找到下一个要被打印的进程信息。数进程，每数一个加len，直到len = pos
	int pcn = 48;
	for (i=0;i<NR_TASKS&&pcn<*pos;i++)
		if(task[i])pcn+=show_entry_len;
	if(i==NR_TASKS)return 0;
	char entry[show_entry_len+1] ; //条目字符串带\0
	for(;i<NR_TASKS&&wn<count;i++){
		if(task[i]){
			sprintf(entry,"%d     %d     %d     %d     %d\n",task[i]->pid,task[i]->father,task[i]->state,task[i]->counter,task[i]->start_time);
			pcn = (*pos-48)%show_entry_len; // 当前条目已经写了多少字节
			pcn = strcp(buf+wn,entry+pcn,show_entry_len-pcn,count-wn,KNFS); // 实际写了多少字节
			wn+=pcn;
			*pos+=pcn;
		}
	}
	return wn;
}

int sys_lseek(unsigned int fd,off_t offset, int origin)
{
	struct file * file;
	int tmp;

	if (fd >= NR_OPEN || !(file=current->filp[fd]) || !(file->f_inode)
	   || !IS_SEEKABLE(MAJOR(file->f_inode->i_dev)))
		return -EBADF;
	if (file->f_inode->i_pipe)
		return -ESPIPE;
	switch (origin) {
		case 0:
			if (offset<0) return -EINVAL;
			file->f_pos=offset;
			break;
		case 1:
			if (file->f_pos+offset<0) return -EINVAL;
			file->f_pos += offset;
			break;
		case 2:
			if ((tmp=file->f_inode->i_size+offset) < 0)
				return -EINVAL;
			file->f_pos = tmp;
			break;
		default:
			return -EINVAL;
	}
	return file->f_pos;
}

int sys_read(unsigned int fd,char * buf,int count)
{
	struct file * file;
	struct m_inode * inode;

	if (fd>=NR_OPEN || count<0 || !(file=current->filp[fd]))
		return -EINVAL;
	if (!count)
		return 0;
	verify_area(buf,count);
	inode = file->f_inode;
	if (inode->i_pipe)
		return (file->f_mode&1)?read_pipe(inode,buf,count):-EIO;
	if (S_ISCHR(inode->i_mode))
		return rw_char(READ,inode->i_zone[0],buf,count,&file->f_pos);
	if (S_ISBLK(inode->i_mode))
		return block_read(inode->i_zone[0],&file->f_pos,buf,count);
	if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
		if (count+file->f_pos > inode->i_size)
			count = inode->i_size - file->f_pos;
		if (count<=0)
			return 0;
		return file_read(inode,file,buf,count);
	}
	if(S_ISPROC(inode->i_mode))
		return proc_read(inode->i_zone[0],buf,count,&file->f_pos);
	printk("(Read)inode->i_mode=%06o\n\r",inode->i_mode);
	return -EINVAL;
}

int sys_write(unsigned int fd,char * buf,int count)
{
	struct file * file;
	struct m_inode * inode;
	
	if (fd>=NR_OPEN || count <0 || !(file=current->filp[fd]))
		return -EINVAL;
	if (!count)
		return 0;
	inode=file->f_inode;
	if (inode->i_pipe)
		return (file->f_mode&2)?write_pipe(inode,buf,count):-EIO;
	if (S_ISCHR(inode->i_mode))
		return rw_char(WRITE,inode->i_zone[0],buf,count,&file->f_pos);
	if (S_ISBLK(inode->i_mode))
		return block_write(inode->i_zone[0],&file->f_pos,buf,count);
	if (S_ISREG(inode->i_mode))
		return file_write(inode,file,buf,count);
	printk("(Write)inode->i_mode=%06o\n\r",inode->i_mode);
	return -EINVAL;
}

