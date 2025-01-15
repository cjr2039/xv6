#include "kernel/types.h"
#include "user/user.h"

/*Requirements:
If the user forgets to pass an argument, sleep should print an error message.
The command-line argument is passed as a string; you can convert it to an integer using atoi (see user/ulib.c).
Use the system call sleep. 
*/

int
main(int argc, char *argv[])
{
    if(argc <= 1)
    {
        printf("ERROR:too few arguments\n");
        exit(1);
    }

    int timesleep = atoi(argv[1]);

    sleep(timesleep);

  exit(0);
}
