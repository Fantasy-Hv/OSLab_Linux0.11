# 实现procfs

在系统中添加/proc/procinfo虚拟文件节点，该文件并不存在于磁盘上，也不是外设，当通过系统调用open/read 读取该文件时，读取的是系统中所有进程的信息。该文件完全存在于内存中，随系统初始化创建。由于读取行为不同，所以需要定义一种新类型来特殊处理对该文件的读取。

##### 添加proc文件类型

在linux-0.11\include\sys\stat.h中添加新类型的掩码和判断宏。

##### 添加创建接口

psinfo 文件结点的创建需要创建inode，这通过 `mknod()` 系统调用建立，所以要让它支持新的文件类型。

直接修改 `fs/namei.c` 文件中的 `sys_mknod()` 函数

##### 在系统初始化时创建文件

系统初始化由main完成，main最后在用户模式下调用init，而init第一件事就是挂载根文件系统，我们的proc文件节点应该就在此后挂载。

1. 建立 `/proc` 目录；建立 `/proc` 目录下的 `psinfo`结点。建立目录和结点分别需要调用 `mkdir()` 和 `mknod()` 系统调用。因为初始化时已经在用户态，所以不能直接调用 `sys_mkdir()` 和 `sys_mknod()`。必须在init()所在文件中实现这两个系统调用的用户态接口
   ```c
   #ifndef __LIBRARY__
   #define __LIBRARY__
   #endif

   _syscall2(int,mkdir,const char*,name,mode_t,mode)
   _syscall3(int,mknod,const char*,filename,mode_t,mode,dev_t,dev)
   ```

    可以将mkdir时proc设置为只有root可读写，mknod时psinfo设置为只读
