#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>

int main(void) {
    int cpid;
    char myFile[20];
    int fd;
    int done, status;

    printf("Please enter a file name: \n");
    fgets(myFile, sizeof(myFile), stdin);
    
    for (int i = 0; i < sizeof(myFile); i++) {
        // Check for ASCII line feed
        if (myFile[i] == 10) {
            myFile[i] = '\0';
            break;
        }
    }
    
    fd = open(myFile, O_WRONLY | O_CREAT | O_TRUNC, 0700);
    
    cpid = fork();
    
    if (cpid == 0) {
        dup2(fd, 1);
        close(fd);
        execlp("ls", "ls", "-l", NULL);	//Execute ls command
    } else {
        printf("Waiting for child %d to finish processing\n", cpid);
        done = wait(&status);
        printf("Child %d finished processing with a status of %d\n", done, status);
    }
}

