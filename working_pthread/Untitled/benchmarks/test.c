#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../worker.h"

void func1() {
    printf("1\n");
}

void func2() {
    printf("2\n");
}

void func3() {
    printf("3\n");
}

/* A scratch program template on which to call and
 * test worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */
int main(int argc, char **argv) {

	/* Implement HERE */
	worker_t* t1;
    worker_t* t2;
    worker_t* t3;

    printf("worker1\n");
    pthread_create(&t1, NULL, (void*)&func1, NULL);
    printf("worker2\n");
    pthread_create(&t2, NULL, (void*)&func2, NULL);
    printf("worker3\n");
    pthread_create(&t3, NULL, (void*)&func3, NULL);
    printf("worker4\n");
	pthread_join(t3, NULL);

	return 0;
}
