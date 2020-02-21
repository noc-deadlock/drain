#include <iostream>

#ifdef __cplusplus
extern "C"
{
#endif
#include <gem5/m5ops.h>

#include "/home/shehab/Work/PhD/gem5/util/m5/m5_mmap.h"

#ifdef __cplusplus
}
#endif

using namespace std;

int main()
{
    map_m5_mem();
    m5_roi_begin();
    cout << "Hello World" << endl;

    return 0;
}
