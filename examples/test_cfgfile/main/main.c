#include <stdio.h>
#include "cfg_file.h"

int main(int argc, char *argv[])
{
    char val[32];

    if(argc < 2)
    {
        printf("usage: %s cfgfile\n", argv[0]);
        return 0;
    }

    if(cfg_file_read(argv[1], "testkey", val, 32) < 0)
    {
        printf("cfg_file_read read error");
        return -1;
    }

    printf("testkey = %s\n", val);
    return 0;
}
