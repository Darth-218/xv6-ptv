#ifndef _PINFO_H
#define _PINFO_H
#define PINFO_UNUSED     0
#define PINFO_USED       1
#define PINFO_SLEEPING   2
#define PINFO_RUNNABLE   3
#define PINFO_RUNNING    4
#define PINFO_ZOMBIE     5

struct pinfo {
    int pid;     // Process ID
    int ppid;    // Parent PID, -1 if no parent
    int state;   // Process state (PINFO_UNUSED=0 .. PINFO_ZOMBIE=5)
    char name[16]; // Process name, null-terminated
};

#endif
