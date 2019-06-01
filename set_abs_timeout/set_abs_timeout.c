#include <stdio.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

void set_abs_timeout(struct timespec *timeout, size_t ms)
{
    if (!timeout) return ;
    size_t sec = 0, usec = 0;
    struct timeval tnow;

    gettimeofday(&tnow, NULL);
    usec = tnow.tv_usec + ms * 1000;
    sec = tnow.tv_sec+usec / 1000000;
    timeout->tv_nsec = (usec % 1000000) * 1000;
    timeout->tv_sec = sec;
}


#if 1
#include <assert.h>
int main()
{
    struct timeval tnow;
    struct timespec timeout;

    gettimeofday(&tnow, NULL);
    set_abs_timeout(&timeout, 1000);
    assert(tnow.tv_sec * 1000 * 1000 * 1000 + tnow.tv_usec * 1000  + 1000 < timeout.tv_sec * 1000 * 1000 * 1000 + timeout.tv_nsec);
    return 0;
}

#endif
