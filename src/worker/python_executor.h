#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
using namespace std;
#define CMD_LEN 3

void execute(string inputfile, string outputfile, string commandfile, char * command, int mode){
    pid_t pid = fork();
    int out_fd = open(outputfile.c_str(), mode, 0666);
    int in_fd = open(inputfile.c_str(), O_RDONLY);
    if(pid == 0){        
        const char * loc = commandfile.c_str();
        dup2(in_fd, 0);
        dup2(out_fd, 1);
        char * const cmd[] = {command, NULL};
        execvp(loc, cmd);
        
    }else{
        int status;
        waitpid(pid, &status,0);
        cout << "Children done!" << endl;
        close(out_fd);
        close(in_fd);
    }
}
