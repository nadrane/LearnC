#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>

#define MAX 25

int main(void) {
    int cpid;
    char line[MAX];
    char myFile[20];
    int fd[2];
    int done, status;
    
    pipe(fd);
    cpid = fork();
    
    if (cpid == 0) {
        printf("The child process is active\n");
        close(fd[1]);
        read(fd[0], line, MAX);
        printf("we received a message: %s\n", line);
    } else {
        printf("Parent process is active\n");
        close(fd[0]);
        char* message = "Your parent is calling";
        write(fd[1], message, 23);
        done = wait(&status);
        printf("Child %d finished processing with a status of %d\n", done, status);
    }
    return 0;
}

