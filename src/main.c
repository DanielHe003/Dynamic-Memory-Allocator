#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include "sfmm.h" // Include your custom memory management functions.

int main() {
	printf("%f %f\n", sf_fragmentation(), sf_utilization());
	void *a = sf_malloc(100);
	sf_free(a);
    return 0;
}