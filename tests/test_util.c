#include <assert.h>
#include <stdio.h>
#include "util.h"

int main(void) {
    assert(add(2, 3) == 5);
    assert(add(0, 0) == 0);
    assert(add(-2, -3) == -5);
    assert(add(-2, 3) == 1);

    printf("all util tests passed\n");
}