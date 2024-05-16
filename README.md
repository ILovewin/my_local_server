# my_local_server

开发一个本地文件服务器，实现客户端与服务器之间的文件传输和交互功能，支持命令发送、文件上传和下载。

## 技术实现

1. **网络通信**：利用Socket编程实现服务器与客户端之间的网络通信，确保数据的可靠传输和交互。
2. **服务器端程序设计**：使用C语言编写服务器端程序，利用epoll多路复用机制管理客户端连接，确保对多个客户端的高效处理。通过线程池技术管理多个客户端的并发请求，避免阻塞主线程，提高服务器的并发处理能力。
3. **客户端程序开发**：编写客户端程序，与服务器进行通信，支持命令发送、文件上传和下载等功能，提供用户友好的交互界面。

## 环境配置

在 Ubuntu 上安装 Raspberry Pi GCC 64 位交叉编译工具链

- 安装必要的软件和工具：运行 sudo apt-get install build-essential git。

- 克隆交叉工具链：切换到 /opt 目录并执行以下命令：

  ```
  cd /opt
  
  sudo git clone git://github.com/raspberrypi/tools.git
  ```

- 可以使用 git pull origin 命令来更新工具链。

- 在 `/opt/tools` 目录中，选择 `gcc-linaro-arm-linux-gnueabihf-raspbian-x64`。

## 快速使用

1. 编译

   本地编译：

   ```
   make
   ```

   Raspberry Pi 64 位交叉编译：

   ```
   make CROSS_COMPILE=1
   ```

2. 运行：打开两个命令行终端，一个运行./server，一个运行./client

3. 客户端与服务端交互：

       1. ls;
       2. mkdir <directory_name>;
       3. cd <directory_path>;
       4. sendfile <file_name>;
       5. receivefile <file_path>;

## 生成调试core文件方法

Core Dump：Core的意思是内存，Dump的意思是扔出来，堆出来（段错误）。开发和使用Unix程序时，有时程序莫名其妙的down了，却没有任何的提示(有时候会提示core dumped)，这时候可以查看一下有没有形如core.进程号的文件生成，这个文件便是操作系统把程序down掉时的内存内容扔出来生成的, 它可以做为调试程序的参考，能够很大程序帮助我们定位问题（[GDB调试参考博客](https://blog.csdn.net/chen1415886044/article/details/105094688?ops_request_misc=%7B%22request%5Fid%22%3A%22170818689616800180669284%22%2C%22scm%22%3A%2220140713.130102334..%22%7D&request_id=170818689616800180669284&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~top_positive~default-1-105094688-null-null.142^v99^pc_search_result_base5&utm_term=gdb调试&spm=1018.2226.3001.4187)）

- 查询核心转储文件的大小限制，若为0，则不会产生对应的coredump，需要进行修改和设置

  ```
  ulimit -c
  ```

- 需要让core文件能够产生，设置core大小为无限

  ```
  ulimit -c unlimited
  ```

- 自己建立一个文件夹，存放生成的core文件，并更改core dump生成路径

  ```
  echo "/home/saob/Documents/my_nas/coredump/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern
  ```

- 编译运行

  ```
  gcc -g -o client client.c
  ```

- gdb 可执行程序

  ```
  gdb /home/saob/Documents/my_nas/client /home/saob/Documents/my_nas/coredump/core.client.6169
  ```

- 查看堆栈跟踪信息

  ```
  (gdb) bt
  #0  __GI_raise (sig=sig@entry=6) at ../sysdeps/unix/sysv/linux/raise.c:50
  #1  0x00007f49f24b8859 in __GI_abort () at abort.c:79
  #2  0x00007f49f252326e in __libc_message (action=action@entry=do_abort, 
      fmt=fmt@entry=0x7f49f264d298 "%s\n") at ../sysdeps/posix/libc_fatal.c:155
  #3  0x00007f49f252b2fc in malloc_printerr (
      str=str@entry=0x7f49f264f5d0 "free(): double free detected in tcache 2")
      at malloc.c:5347
  #4  0x00007f49f252cf6d in _int_free (av=0x7f49f2682b80 <main_arena>, 
      p=0x560bd428eab0, have_lock=0) at malloc.c:4201
  #5  0x0000560bd3457b69 in main () at client.c:213
  ```

## 改进

为避免空闲客户端一直占用服务器资源，引入了定时器处理非活动连接，并重新封装线程池，增加了任务队列操作，以提高服务器的性能（参考https://github.com/qinguoyi/TinyWebServer）