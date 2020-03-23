#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

struct t_format {
    long long s;
    long long us;
};

struct time_message {
    struct t_format t;
    char buff[500];
    uint8_t voice_buff[1024];
};

static struct t_format gettime () {
    struct timeval tval;
    gettimeofday (&tval, NULL);
    long long s = tval.tv_sec, us = tval.tv_usec;
    // ll tus = s*(1e6)+us;
    // tus += us_off;
    // s = tus/(1e6);
    // us = tus-(s*(1e6));

    // us += us_off;
    struct t_format t = {s, us};
    return t;
}

static long long timediff (struct t_format t1, struct t_format t2) {
    t2.s *= (1e6);
    t1.s *= (1e6);

    // printf ("%lld %lld\n", t1.s, t1.us);
    return (t2.s-t1.s) + (t2.us-t1.us);
}
