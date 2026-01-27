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

##### 编写读取函数

至此已经可以在shell中查看到/proc/psinfo文件，但因为是新的文件类型并且未设置对应的读写处理函数因此读取会闹错。接下来实现该文件的功能，修改fs/read_write.c中的sys_read，添加对应的条件判断和函数实现。proc 文件的处理函数的功能是根据设备编号，把不同的内容写入到用户空间的 buf。写入的数据要从 `f_pos` 指向的位置开始，每次最多写 count 个字节，并根据实际写入的字节数调整 `f_pos` 的值，最后返回实际写入的字节数。当设备编号表明要读的是 psinfo 的内容时，就要按照 psinfo 的形式组织数据。需要传给处理函数的参数包括：

- inode->i_zone[0]，这就是 mknod() 时指定的 dev ——设备编号,这里设置成4或6，其他值可能报错
- buf，指向用户空间，就是 read() 的第二个参数，用来接收数据
- count，就是 read() 的第三个参数，说明 buf 指向的缓冲区大小
- &file->f_pos，f_pos 是上一次读文件结束时“文件位置指针”的指向。这里必须传指针，因为处理函数需要根据传给 buf 的数据量修改 f_pos 的值。
