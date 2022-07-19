#include "webserver.h"

int Utils::u_pipefd=0;

void Utils::addfd(int epollfd,int fd,int trig_mode){
    epoll_event event;
    event.data.fd = fd;

    if (1 == trig_mode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::setnonblocking(int fd){
    int old_op=fcntl(fd,F_GETFL);
    int new_op=old_op|O_NONBLOCK;
    fcntl(fd, F_SETFL, new_op);
}

void Utils::sig_handle(int sig){
    int erro=errno;
    send(u_pipefd,(char*)&sig,1,0);
    errno=erro;
}

void Utils::addsig(int sig,void (handle)(int)){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handle;
    sigfillset(&sa.sa_mask);
    if(sigaction(sig, &sa, NULL) == -1){
        printf("sigaction wrong!\n");
        exit(1);
    }
}


void webserver::init(int port , string user, string passWord, string databaseName,int opt_linger, int trigmode, int sql_num,int thread_num){
    m_port=port;
    m_user=user;
    m_passWord=passWord;     
    m_databaseName=databaseName; 
    m_OPT_LINGER=opt_linger;
    m_TRIGMode=trigmode;
    m_sql_num=sql_num;
    m_thread_num=thread_num;

    
    users = new http_conn[MAX_FD];
    users_timer = new client[MAX_FD];

    
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

   
    http_conn::doc_root=m_root;
    
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num);
    http_conn::initmysql_result(m_connPool);

    m_pool = new threadpool(m_connPool, m_thread_num);

     //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
    else{
        printf("wrong trig_mode!\n");
        exit(1);
    }
    http_conn::m_TRIGMode=m_CONNTrigmode;
}

void webserver::eventListen(){

    //listenfd创建
    m_listenfd=socket(PF_INET,SOCK_STREAM,0);
    if(m_listenfd<0){
        printf("create socket wrong!");
        exit(1);
    }
    //linger设置
    if(m_OPT_LINGER==1){
        struct linger tem={1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tem,sizeof(tem));
    }
    else{
        if(m_OPT_LINGER!=0){
            printf("LINGER option set wrong!\n");
            exit(1);
        }
    }
    //reuse设置
    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));

    //bind&&listen
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);

    if(bind(m_listenfd,(struct sockaddr*)&address,sizeof(address))<0){
        printf("bind wrong!\n");
        exit(1);
    }
    if(listen(m_listenfd,5)<0){
        printf("listen wrong!\n");
        exit(1);
    }


    //epoll create
    m_epollfd=epoll_create(5);
    if(m_epollfd==-1){
        printf("epoll tree create wrong!\n");
        exit(1);
    }
    http_conn::m_epollfd=m_epollfd;
    utils.m_time_sorter.t_epollfd=m_epollfd;

    //listenfd挂载、pipe创建和挂载
    utils.addfd(m_epollfd,m_listenfd,m_LISTENTrigmode);

    if(socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd)==-1){
        printf("socket pair create wrong!\n");
        exit(1);
    }
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd,m_pipefd[0],0);

    Utils::u_pipefd=m_pipefd[1];

    //注册信号
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM,utils.sig_handle);
    
    //启动定时
    alarm(TIMESLOT);
    
}

void webserver::eventLoop(){
    bool timeout=false;
    while(true){
        int num=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);

        if (num < 0 && errno != EINTR)
        {
            printf("epoll failure/n");
            exit(1);
        }

        for(int i=0;i<num;i++){

            int socketfd=events[i].data.fd;

            if(socketfd==m_listenfd){
                dealclinetdata();
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                if(users_timer[socketfd].m_timer!=NULL){
                    users[socketfd].close_conn();
                    utils.m_time_sorter.del_timer(users_timer[socketfd].m_timer);
                }
            }
            else if(socketfd==m_pipefd[0]&&(events[i].events & EPOLLIN)){
                dealwithsignal(timeout);
            }
            else if(events[i].events & EPOLLIN){
                dealwithread(socketfd);
            }
            else if(events[i].events & EPOLLOUT){
                dealwithwrite(socketfd);
            }
            else{
                printf("unknown situation happened in event loop!\n");
                exit(1);
            }
        }

        if(timeout){
            utils.m_time_sorter.tick();
            printf("time tick!\n");
            alarm(TIMESLOT);
            timeout=false;
        }
    }
}



void webserver::dealclinetdata(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    if(m_LISTENTrigmode==0){
       int conn_fd = accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
        // printf("acceptfd: %d\n",conn_fd);
       if(conn_fd<0){
            printf("accept wrong in LT mode!\n");
            return;
       }

       if(http_conn::m_user_count>=MAX_FD){
       const char * tem="Internal server busy";
        send(conn_fd,tem,strlen(tem),0);
        close(conn_fd);
        return;
       }

       timer1(conn_fd,client_address);
    }
    else{

        while(true){

            int conn_fd = accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);

            if(conn_fd<0){
                 return;
            }

            if(http_conn::m_user_count>=MAX_FD){
                const char * tem="Internal server busy";
                send(conn_fd,tem,strlen(tem),0);
                close(conn_fd);
                return;
            }

            timer1(conn_fd,client_address);
        }

    }
    
}


void webserver::timer1(int connfd, struct sockaddr_in client_address){
    users[connfd].init(connfd,client_address);

    users_timer[connfd].fd=connfd;
    users_timer[connfd].address=client_address;
    users_timer[connfd].m_timer=new timer(&users_timer[connfd],time(NULL)+3*TIMESLOT);

    utils.m_time_sorter.add_timer(users_timer[connfd].m_timer);

}

void webserver::dealwithsignal(bool& timeout){

    char signal[1024];
    int ret;
    if((ret=recv(m_pipefd[0],signal,sizeof(signal),0))>0){
        for(int i=0;i<ret;i++){
            if(signal[i]==SIGALRM){
                timeout=true;
                break;
            }
        }
    }
    else{
        printf("dealwithsignal wrong!\n");
    }
}

void webserver::dealwithread(int sockfd){
    if(users[sockfd].read_once()){


        m_pool->append(users+sockfd);

        time_t cur = time(NULL);
        users_timer[sockfd].m_timer->expire = cur + 3 * TIMESLOT;
        utils.m_time_sorter.adjust_timer(users_timer[sockfd].m_timer);
    }
    else{
        users[sockfd].close_conn();
        utils.m_time_sorter.del_timer(users_timer[sockfd].m_timer);
    }
}

void webserver::dealwithwrite(int sockfd){
    if(users[sockfd].write()){

        time_t cur = time(NULL);
        users_timer[sockfd].m_timer->expire = cur + 3 * TIMESLOT;
        utils.m_time_sorter.adjust_timer(users_timer[sockfd].m_timer);

    }
    else{

        users[sockfd].close_conn();
        utils.m_time_sorter.del_timer(users_timer[sockfd].m_timer);

    }
}


