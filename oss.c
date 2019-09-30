#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

//Globals
enum FLAGS {
    HELP_FLAG,
    SPAWN_FLAG,
    LOG_FLAG,
    TIME_FLAG,
    TOTAL_FLAGS
};

const int DEBUG = 1;

//Function Prototypes
void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime);
void printIntArray(int* arr, int size);

//--------

int main(int argc, char* argv[]) {
    //Program variables
    int maxChildren = 5;
    char* logFileName = "log.txt";
    int terminateTime = 5;
    int i;
    int status;

    handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);

    if(DEBUG) {
        printf("maxChildren = %d\n", maxChildren);
        printf("logFileName = %s\n", logFileName);
        printf("terminateTime = %d\n\n", terminateTime);
    }

    printf("Parent before fork: %d\n", getpid());

    //Spawn a fan of maxChildren # of processes
    pid_t pid = 0;
    for(i = 1; i < maxChildren; i++) {
        pid = fork();

        //Fork error.
        if(pid < 0) {
            perror("ERROR:oss:Failed to fork");
            exit(1);
        }

        //Process child only.
        if(pid == 0) {
            sleep(1);
            printf("Child:%d, says hello to parent:%d\n", getpid(), getppid());
            exit(0);
        }
        
        //Process parent only.
        if(pid > 0) {
            fprintf(stderr, "onPass:%d, Parent created child process: %d\n", i, pid);
        }
    }
        
    for(i = 1; i < maxChildren; i++) {
        sleep(2);
        wait(&status);
        printf("%d: exit status: %d\n", i, WEXITSTATUS(status));
    }

    return 0;
}

//--------

//Function definitions.

void handleArgs(int argc, char* argv[], int* maxChild, char** logFile, int* termTime) {
    int i;
    int flagArray[TOTAL_FLAGS];

    //Initialize the flag array to 0.
    for(i = 0; i < TOTAL_FLAGS; i++)
        flagArray[i] = 0;
        
    if(DEBUG) {
        printf("flagArray initialized to:\n");
        printIntArray(flagArray, TOTAL_FLAGS);
    }

    //Parse each argument from the command line.
    int arg = -1;
    while((arg = getopt(argc, argv, "hs:l:t:")) != -1) {
        switch(arg) {
            case 'h':
                flagArray[HELP_FLAG] = 1;
                break;
            case 's':
                flagArray[SPAWN_FLAG] = 1;
                *maxChild = atoi(optarg);
                break;
            case 'l':
                flagArray[LOG_FLAG] = 1;
                *logFile = optarg;
                break;
            case 't':
                flagArray[TIME_FLAG] = 1;
                *termTime = atoi(optarg);
                break;
            case '?':
                printf("ERROR:oss:Invalid CLI arguments\nProgram Exited");
                exit(1);
                break;
        }
    }


}

void printIntArray(int* arr, int size) {
    int i;
    for(i = 0; i < size; ++i) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}