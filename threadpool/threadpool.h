#ifndef THREAD_H
#define THREAD_H
#include <list>
#include <pthread.h>
#include "../mysql/con_pool.h"
#include "../lock/lock.h"
#include "../http/http.h"
#include <stdlib.h>
class threadpool
{
public:
    threadpool(connection_pool *connPool, int thread_number = 4, int max_request = 10000);
    ~threadpool();
    bool append(http_conn *request);

private:

    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        
    int m_max_requests;         
    pthread_t *m_threads;       
    std::list<http_conn *> m_workqueue; 
    locker m_queuelocker;      
    sem m_queuestat;            
    connection_pool *m_connPool;          
};



#endif