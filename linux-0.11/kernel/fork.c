/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);

long last_pid=0;

void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

	code_limit=get_limit(0x0f);
	data_limit=get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
extern void first_ret_from_kernel(void);
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;
	//printk("call fork...%d\n",nr);
	p = (struct task_struct *) get_free_page(); // 为新进程的pcb申请一页内存
	if (!p)
		return -EAGAIN;
	task[nr] = p;
	*p = *current;	/* 浅拷贝父进程的大部分信息到新进程，不会复制内核栈 */
	// 设置进程状态与基本信息
	p->state = TASK_UNINTERRUPTIBLE; //不可睡眠
	p->pid = last_pid; 
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
		if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	// 	为新进程复制父进程内存空间（代码、数据、堆栈）
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}

	// 初始化内核栈内容
	long * kernelstack = (long *) (PAGE_SIZE + (long) p);
	// 给内核栈做成能用iret返回的样子
	*(--kernelstack) = ss & 0xffff;
	*(--kernelstack) = esp;
	*(--kernelstack) = eflags;
	*(--kernelstack) = cs & 0xffff;
	*(--kernelstack) = eip;
	*(--kernelstack) = ds & 0xffff;
	*(--kernelstack) = es & 0xffff;
	*(--kernelstack) = fs & 0xffff;
	*(--kernelstack) = gs & 0xffff;
	*(--kernelstack) = esi;
	*(--kernelstack) = edi;
	*(--kernelstack) = edx;
	*(--kernelstack) = first_ret_from_kernel; 
	*(--kernelstack) = ebp;
	*(--kernelstack) = ebx;
	*(--kernelstack) = ecx;
	*(--kernelstack) = 0; // 为子进程返回0
	p->kernelstack = kernelstack;
	printk("kernlstack inited\n");

	// 复制打开文件
	for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]))
			f->f_count++;
	// 复制文件系统信息		
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	//  在 GDT 中注册 TSS 和 LDT ，这两个东西也占用内存段,故为它们设置段描述符
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss)); // tss 的段描述符
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt)); // ldt 的段描述符
	p->state = TASK_RUNNING;	/* do this last, just in case */
	return last_pid; 
}


int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
