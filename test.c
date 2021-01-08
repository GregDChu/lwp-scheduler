#include <stdio.h>
#include "lwp.h"

void fib(int *two)
{
    int i = 0;
    while(i < 10)
    {
        int a = two[0];
        two[0] = two[1];
        two[1] = a + two[0];
        fprintf(stdout, " %d ", two[1]);
        lwp_stop();
        i++;
        lwp_yield();
    }
    fprintf(stdout, "\nThread %d says adios...\n", (int)lwp_gettid());
    lwp_start();
    lwp_yield();
    return;
}

int main()
{
    int two[2];
    two[0] = 0;
    two[1] = 1;
    lwp_start();
    lwp_set_scheduler(NULL);
    tid_t t1 = lwp_create((lwpfun)fib, &two, 2048);
    tid_t t2 = lwp_create((lwpfun)fib, &two, 2048);
    fprintf(stdout, "ID: %d\n", (int)tid2thread(t1)->tid);
    fprintf(stdout, "ID: %d\n", (int)tid2thread(t2)->tid);
    fprintf(stdout, " 0  1 ");
    lwp_start();
    lwp_yield();
    lwp_exit();
    int i = 0;
    while(i < 5)
    {
        lwp_start();
        i++;
    }
    fprintf(stdout, "\nWe have reached a halt...\n");
    return 0;
}