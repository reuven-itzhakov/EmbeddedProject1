#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    mkdir("/tmp/sysmon", 0777);
    
    return 0;
}