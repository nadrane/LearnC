#include<stdio.h>
#include<fcntl.h>
#include<string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>


#define OK       0
#define NO_INPUT 1
#define TOO_LONG 2
static int GetLine (char *prmpt, char *buff, unsigned int sz) {
        int ch, extra;
    
        // Get line with buffer overrun protection.
        if (prmpt != NULL) {
            printf("%s", prmpt);
            fflush(stdout);
        }
    
        if (fgets (buff, sz, stdin) == NULL) {
            fprintf(stderr, "fgets returned NULL");
            return NO_INPUT;
        }
     
        // If it was too long, there'll be no newline. In that case, we flush
        // to end of line so that excess doesn't affect the next call.
        if (buff[strlen(buff)-1] != '\n') {
            extra = 0;
            while (((ch = getchar()) != '\n') && (ch != EOF)) {
                extra = 1;
            }
            return (extra == 1) ? TOO_LONG : OK;
        }
     
        // Otherwise remove newline and give string back to caller.
        buff[strlen(buff)-1] = '\0';
        return OK;
}


int main() {
    char sourceFile[100];
    char destinationFile[100];
    int error, bytesRead;
    int sourceFD, destinationFD;
    char buffer[1000];
    
    error = GetLine("Please enter a source file name: \n", sourceFile, 100);
    if (error == 1) {
        printf("A source file was not inputted.\n");
        return 0;
    }
    else if (error == 2) {
        printf("Source file is too long.\n");
        return 0;
    }

    error = GetLine("Please enter a destination file name: \n", destinationFile, 100);
    if (error == 1) {
        printf("A destination file was not inputted.\n");
        return 0;
    }
    else if (error == 2) {
        printf("Destination file is too long.\n");
        return 0;
    }
    
    sourceFD = open(sourceFile,  O_RDONLY);
    
    if (sourceFD < 0) {
        printf("Source file failed to open\n");
        return 0;
    }
    
    destinationFD = open(destinationFile, O_WRONLY | O_CREAT | O_TRUNC, 0700);
    if (destinationFD < 0) {
        printf("Destination file failed to open\n");
        return 0;
    }
    
    //100 bytes is way too small, but I don't have too many big files
    bytesRead = read(sourceFD, buffer, 100);
    while (bytesRead) {
        write(destinationFD, buffer, bytesRead);
        bytesRead = read(sourceFD, buffer, 100);
    }
}