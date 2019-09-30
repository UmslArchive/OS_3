#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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

    handleArgs(argc, argv, &maxChildren, &logFileName, &terminateTime);

    if(DEBUG) {
        printf("maxChildren = %d\n", maxChildren);
        printf("logFileName = %s\n", logFileName);
        printf("terminateTime = %d\n", terminateTime);
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