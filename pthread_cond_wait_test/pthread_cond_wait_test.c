#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

pthread_mutex_t test_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t test_cond=PTHREAD_COND_INITIALIZER;

void *test_thread(void *arg)
{

    for (int i = 0; i < 1 << 31; i++)
        for (int j = 0; j < 1 << 31; j++)
            for (int k = 0; k < 1 << 31; k++);
    printf("my thread id is %ld, after waste time\n",pthread_self());

    pthread_mutex_lock(&test_mutex);
    pthread_cond_wait(&test_cond,&test_mutex);
    pthread_mutex_unlock(&test_mutex);
    printf("my thread id is %ld, thread exit\n",pthread_self());
    return arg;

}

int  main(void )
{
    pthread_t th;
    pthread_create(&th, NULL, test_thread, NULL);

    pthread_cond_signal(&test_cond);
    printf("after send signal\n");

    sleep(5);
    printf("after sleep\n");

    printf("send signal again\n");
    pthread_cond_signal(&test_cond);
    pthread_join(th, NULL);

    return 0;
}
