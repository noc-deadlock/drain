#include <stdio.h>

#include <gem5/m5ops.h>

#include "/home/shehab/Work/PhD/gem5/util/m5/m5_mmap.h"

int main()
{
    map_m5_mem();
    m5_roi_begin();
    printf("Hello\n");

    return 0;
}
