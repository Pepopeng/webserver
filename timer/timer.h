#ifndef TIME_H
#define TIME_H
#include <sys/socket.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "../http/http.h"

class timer;
struct client
{
    sockaddr_in address;
    int fd;
    timer* m_timer;
};


class timer
{
public:
    timer* prev;
    timer* next;
    time_t expire;
    client* m_client;
public:
    timer(client* client,time_t expire):prev(NULL),next(NULL),m_client(client),expire(expire){}

    ~timer(){}
};

class time_sorter
{
private:
    timer* head;
    timer* tail;
    void add_timer(timer* _timer,timer* _head);
public:
    time_sorter();
    ~time_sorter();
    void add_timer(timer* _timer);
    void del_timer(timer* _timer);
    void adjust_timer(timer* _timer);
    void tick();
    int t_epollfd;
};


#endif



