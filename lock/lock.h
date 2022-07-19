#ifndef LOCK_H
#define LOCK_H
#include <semaphore.h>
#include <pthread.h>
#include <iostream>
#include <cstdlib>

class locker
{
private:
    pthread_mutex_t m_mutex;
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL)!=0){
            std::cout<<"lock init wrong!"<<std::endl;
            exit(1);
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);
    }
    void lock(){
        pthread_mutex_lock(&m_mutex);
    }
    void unlock(){
        pthread_mutex_unlock(&m_mutex);
    }

};



class sem
{
private:
    sem_t m_sem;
public:
    sem(){
        if(sem_init(&m_sem,0,0)!=0){
            std::cout<<"sem init wrong!"<<std::endl;
            exit(1);
        }
    }
    sem(int num){
        if(sem_init(&m_sem,0,num)!=0){
            std::cout<<"sem init1 wrong!"<<std::endl;
            exit(1);
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    void wait(){
        sem_wait(&m_sem);
    }
    void post(){
        sem_post(&m_sem);
    }
};

#endif



