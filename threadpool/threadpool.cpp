#include "threadpool.h"


threadpool::threadpool(connection_pool *connPool, int thread_number, int max_request){

    m_connPool=connPool;
    m_thread_number=thread_number;
    m_max_requests=max_request;

    m_threads=new pthread_t[thread_number];
    for(int i=0;i<thread_number;i++){
        if(pthread_create(m_threads+i,NULL,worker,this)!=0){
            printf("problem occur when create thread!\n");
            exit(1);
        }
        if(pthread_detach(m_threads[i])){
            printf("problem occur when detach thread!\n");
            exit(1);
        }

    }
}


threadpool::~threadpool(){
    delete[] m_threads;
}


bool threadpool::append(http_conn *request){
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}


void* threadpool::worker(void * arg){
    threadpool* pool=(threadpool*) arg;
    pool->run();
    return pool;
}


void threadpool::run(){
    while(true){
        m_queuestat.wait();

        while(true){
            m_queuelocker.lock();

            if(m_workqueue.empty()){
                m_queuelocker.unlock();
                break;
            }
            // printf("empty: %d\n",m_workqueue.empty());
            // printf("queue size: %d\n",m_workqueue.size());
            http_conn* request=m_workqueue.front();
            m_workqueue.pop_front();

            m_queuelocker.unlock();
            
            if(!request){
                break;
            }
            request->mysql=m_connPool->GetConnection();
            // printf("为null? %d\n",request->mysql==NULL);
            // printf("freeconn:  %d\n",m_connPool->GetFreeConn());
            request->process();
            
            m_connPool->ReleaseConnection(request->mysql);
            request->mysql=NULL;
        }
    }
}

