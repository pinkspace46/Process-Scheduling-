#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "process_control.h"
#include "scheduler.h"

#define SYS_PRINTSTRING 333

typedef struct sched_param Sched_pm;
typedef struct timespec Time_sp;

/* one time unit = the time it takes 
   to run this functio once */
void inline TIME_UNIT(void){
    /* volatile means treat the variable
       as if it may change without the 
       compiler knowing it */
    volatile unsigned long i;
    /* loop one million times to kill time
       UL means unsigned long */
    for(i = 0; i < 1000000UL; i++);
    return;
}

/* this function makes pid only run on core */
int assign_core(pid_t pid, int core){
    cpu_set_t cpu_mask;

    /* it's an error if core is greater than or equal to
       the number of cpus permitted by the system */
    if (core >= sizeof(cpu_mask)){
        fprintf(stderr, "Invalid Core No.: %d\n", core);
        return -1;
    }

    /* clear cpu_mask */
    CPU_ZERO(&cpu_mask);
    /* set the flag on cpu */
    CPU_SET(core, &cpu_mask);

    /* assign pid on cpu indicated by core */
    if ( sched_setaffinity(pid, sizeof(cpu_mask), &cpu_mask) != 0 ){
        perror("error: sched_setaffinity");
        exit(-1);
    }

    return 0;
}

/* function used to create and run child process */
pid_t proc_create(Process chld){
    /* fork a child process */
    pid_t chpid = fork();
    /* fork error */
    if ( chpid < 0 ){
        perror("error: fork");
        exit(2);
    }

    /* chpid == 0 means child process */
    if ( chpid == 0 ){ // emulate child processes
        /* close the pipe write file descriptor for the child */
        close( chld.pipe_fd[1] );
        
        /* record the initial child execution time */
        int init_exec_time = chld.exec_time;

        /* declare two time structs */
        Time_sp start, end;
        /* system message buffer */
        char dmesg[256] = "";

        /* loop if there's time left */
        while( chld.exec_time > 0 ){
            // synchronize with scheduler
            char buf[8];
            /* use a blocking system call (read)
               to synchronize with the scheduler */
            read(chld.pipe_fd[0], buf, strlen("run"));
            
            /* if it is the first time being run */
            if( chld.exec_time == init_exec_time ){
                /* get the system time */
                if( clock_gettime(CLOCK_REALTIME, &start) == -1 ){
                    perror("error: clock_gettime");
                    exit(3);
                }
                /* print out child name and its pid */
                printf("%s %d\n", chld.name, getpid());
            }
            
            /* run the child process for a time unit */
            TIME_UNIT();
            /* decrement child's execution time */
            chld.exec_time--;

/* this gets called if we define PRINT_CHLD */
#ifdef PRINT_CHLD
            fprintf(stderr, "%s, rounds left %d\n", chld.name, chld.exec_time);
#endif
        }
        /* get system time, when the while loop finishes */
        if( clock_gettime(CLOCK_REALTIME, &end) == -1 ){
            perror("error: clock_gettime");
            exit(3);
        }

        /* place pid, start and end time into a char buffer 'dmesg' */
        sprintf(dmesg, "[Project1] %d %09lu.%09lu %09lu.%09lu\n",
                getpid(), start.tv_sec, start.tv_nsec, end.tv_sec, end.tv_nsec);

/* this gets called if we define PRINT_CHLD */
#ifdef PRINT_CHLD
	printf("len of dmesg: %d\n", (int)strlen(dmesg));
#endif
        /* use a self defined system call to write dmesg */
        syscall(SYS_PRINTSTRING, strlen(dmesg)+1, dmesg);
        /* child process exits */
        exit(0);
    }
    
    
    /* this part is what the parent process runs
       it runs before the child's 'read' call 
       gets executed */

    /* set it to very low priority */
    proc_kickout(chpid);
    /* assign chpid to cpu CHILD_CORE(1) */ 
    assign_core(chpid, CHILD_CORE);
    /* close pipe read descriptor of the parentf */
    close( chld.pipe_fd[0] );

    /* return pid of the child */
    return chpid;
}

/* this function sets pid to the lowest prioity possible */
int proc_kickout(pid_t pid){
    Sched_pm sp;
    /* 0 is normal priority */
    sp.sched_priority = 0;

    /* set process to the lowest priority (IDLE) */
    if ( sched_setscheduler(pid, SCHED_IDLE, &sp) < 0 ){
        perror("error: sched_setscheduler");
        return -1;
    }

    return 0;
}

/* set pid to OTHER priority group */
int proc_resume(pid_t pid){
    Sched_pm sp;
    sp.sched_priority = 0;

    if ( sched_setscheduler(pid, SCHED_OTHER, &sp) < 0 ){
        perror("error: sched_setscheduler");
        return -1;
    }

    return 0;
}
