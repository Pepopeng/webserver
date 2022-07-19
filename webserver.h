#ifndef WEBSERVE_H
#define WEBSERVE_H
#include "./http/http.h"
#include "./timer/timer.h"
#include "./mysql/con_pool.h"
#include "./threadpool/threadpool.h"
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include<fcntl.h>
#include <signal.h>
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;  
const int MAX_FD = 65530; 

class Utils{
    public:
        Utils(){}
        ~Utils(){}

        void addfd(int epollfd,int fd,int trig_mode);
        void setnonblocking(int fd);
        void addsig(int sig,void (handle)(int));
       static  void sig_handle(int sig);

        static int u_pipefd;
        time_sorter m_time_sorter;

};



class webserver
{
public:
    webserver(){}
    ~webserver(){
        close(m_epollfd);
        close(m_listenfd);
        close(m_pipefd[1]);
        close(m_pipefd[0]);
        delete[] users;
        delete[] users_timer;
        delete m_pool;
        free(m_root);
    }

    void init(int port , string user, string passWord, string databaseName,int opt_linger, 
              int trigmode, int sql_num,int thread_num);
    void eventListen();
    void eventLoop();

    void timer1(int connfd, struct sockaddr_in client_address);
    
    void dealclinetdata();
    void dealwithsignal(bool& timeout);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础
    int m_port;
    char *m_root;

    int m_pipefd[2];
    int m_epollfd;

    http_conn *users;

    //数据库相关
    connection_pool *m_connPool;
    string m_user;         
    string m_passWord;     
    string m_databaseName; 
    int m_sql_num;

    //线程池相关
    threadpool *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    client *users_timer;
    Utils utils;
};

#endif

