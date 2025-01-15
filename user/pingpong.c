// //写这个实现的时候满脑子重定向+printf，都忘了用fprintf......
// //太臃肿太混乱了
// #include "kernel/types.h"
// #include "user/user.h"
// #include "kernel/fcntl.h"

// int
// main(int argc, char *argv[])
// {
//     int pipe0[2];//父写子读
//     int pipe1[2];//子写父读
  
//     if(pipe(pipe0)<0 || pipe(pipe1)<0)
//     {
//         printf("ERROR:pipe");
//         exit(-1);
//     }

//     if(fork() == 0)
//     {
//         // child接受“p”
//         close(0);//redir
//         dup(pipe0[0]);//pipe0的读管道一定会重定向到标准输入fd0
//         close(pipe0[0]);
//         close(pipe0[1]);
//         char bufc = {0};
//         read(0,&bufc,1);

//         // 只有正确接收parent发的byte，child才发送“c”
//         if(bufc == 'p')
//          {
//            printf("%d: received ping\n",getpid());

//         close(1);//redir
//         dup(pipe1[1]);//默认最小可用文件fd，因此pipe1的写入管道一定会重定向到标准输出fd1
//         close(pipe1[0]);//用不着，直接关
//         write(1,"c",1);
//         close(pipe1[1]);//发送完了才能关pipe0[1]

//             exit(0);
//           }
//         else
//         {
//             printf("Error:Child read a wrong byte:%c,Child Send canceled",bufc);
//             exit(-1);
//         }
//     }
//     else{
//         // parent发送“p”
//         close(1);//redir
//         dup(pipe0[1]);//默认最小可用文件fd，因此pipe0的写入管道一定会重定向到标准输出fd1
//         close(pipe0[0]);//用不着，直接关
//         write(1,"p",1);
//         close(pipe0[1]);//发送完了才能关pipe0[1]

//         wait(0); //先让子进程打印，子进程发送并返回后，再让父进程接收并打印

//         // parent接收“c”
//         close(0);//redir
//         dup(pipe1[0]);//pipe1的读管道一定会重定向到标准输入fd0
//         close(pipe1[0]);
//         close(pipe1[1]);
//         char bufp = {0};
//         read(0,&bufp,1);
//         // 只有正确接收child发的byte，才打印
//         if(bufp == 'c')
//         {
//             close(1);
//             open("console", O_RDWR);
//             printf("%d: received pong\n",getpid());
//         }
            
//     }
//     exit(0);
// }



//下面这才是正常的实现
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
    // parent to child
    int fd[2];

    if (pipe(fd) == -1) {
        fprintf(2, "Error: pipe(fd) error.\n");
    }
    // child process
    if (fork() == 0){
        char buffer[1];
        read(fd[0], buffer, 1);
        close(fd[0]);
        fprintf(0, "%d: received ping\n", getpid());
        write(fd[1], buffer, 1);
        close(fd[1]);
    }
    // parent process
    else {
        char buffer[1];
        buffer[0] = 'a';
        write(fd[1], buffer, 1);
        close(fd[1]);
        read(fd[0], buffer, 1);
        fprintf(0, "%d: received pong\n", getpid());
        close(fd[0]);
    }
    exit(0);
}