#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>

#include "sharedMemoryKeys.h"

int DEBUG = 1;

int main(int argc, char* argv[]) {

    fprintf(stderr, "arg1 %s", argv[1]);

    //Shared memory vars
    key_t ckey = MSG_KEY;
    size_t size = 0;
    int cshmid;
    char* cshmPtr = NULL;
    int* cshmIntPtr = NULL;

    if(argc == 2) {
        size = (unsigned long) atoi(argv[1]);
    }

    if(DEBUG) {
        fprintf(stderr, "Child:%d, says hello to parent:%d\n", getpid(), getppid());
        fprintf(stderr, "Passed Size: %ld\n", size);
    }

    sleep(1);

    //Fetch the segment id
    cshmid = shmget(ckey, size, 0777);
    if(cshmid < 0) {
        perror("ERROR:usrPs:shmget failed");
        fprintf(stderr, "size = %ld\n", size);
        exit(1);
    }

    //Attach to segment
    cshmPtr = (char*)shmat(cshmid, 0, 0);
    if(cshmPtr == (char*) -1) {
        perror("ERROR:usrPs:shmat failed");
        exit(1);
    }
    cshmIntPtr = (int*)cshmPtr;

    //Read the shared memory
    fprintf(stderr, "Child %d reads: ", getpid());
    int i;
    for(i = 0; i < size / sizeof(int); ++i) {
        fprintf(stderr, "%d ", *cshmIntPtr++);
    }
    fprintf(stderr, "\n");

    return 50;
}