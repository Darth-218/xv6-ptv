#ifndef _PINFO_H
#define _PINFO_H

struct pinfo {
    int pid;     // Process ID
    int ppid;    // Parent PID, -1 if no parent
    int state;   // Process state (enum procstate: 0=UNUSED .. 5=ZOMBIE)
    char name[16]; // Process name, null-terminated
};

#endif
