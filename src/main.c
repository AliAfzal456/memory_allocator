#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    sf_mem_init();

    void *one = sf_malloc(1);
    void *two = sf_malloc(2);
    void *three = sf_malloc(3);
    void *four = sf_malloc(4);
    void *five = sf_malloc(5);
    sf_malloc(1);

    sf_free(one);
    sf_free(two);
    sf_free(three);
    sf_free(four);
    sf_free(five);
    sf_snapshot();
    sf_mem_fini();

    return EXIT_SUCCESS;
}
