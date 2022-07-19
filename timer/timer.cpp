#include "timer.h"


void time_sorter::add_timer(timer* _timer,timer* _head){
    timer *prev = _head;
    timer *tmp = prev->next;
    while (tmp)
    {
        if (_timer->expire < tmp->expire)
        {
            prev->next = _timer;
            _timer->next = tmp;
            tmp->prev = _timer;
            _timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = _timer;
        _timer->prev = prev;
        _timer->next = NULL;
        tail = _timer;
    }

}


void time_sorter::add_timer(timer* _timer){
    if(!head){
        head=tail=_timer;
        return;
    }
    if(head->expire>_timer->expire){
        _timer->next=head;
        head->prev=_timer;
        head=_timer;
        return;
    }

    add_timer(_timer,head);

}


void time_sorter::del_timer(timer* _timer){
    if(head==_timer&&tail==_timer){
        head=NULL;
        tail=NULL;
        delete _timer;
        return;
    }
    if(head==_timer){
        head=head->next;
        head->prev=NULL;
        delete _timer;
        return;
    }
    if(tail==_timer){
        tail=tail->prev;
        tail->next=NULL;
        delete _timer;
        return;
    }
    _timer->prev->next=_timer->next;
    _timer->next->prev=_timer->prev;
    delete _timer;
}

void time_sorter::adjust_timer(timer* _timer){
    timer *tmp = _timer->next;
    if (!tmp || (_timer->expire < tmp->expire))
    {
        return;
    }
    if (_timer == head)
    {
        head = head->next;
        head->prev = NULL;
        _timer->next = NULL;
        add_timer(_timer, head);
    }
    else
    {
        _timer->prev->next = _timer->next;
        _timer->next->prev = _timer->prev;
        add_timer(_timer, _timer->next);
    }
}

time_sorter::time_sorter(){
    head=NULL;
    tail=NULL;
}

time_sorter::~time_sorter(){
    timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void time_sorter::tick(){
    if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);
    timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }

        epoll_ctl(t_epollfd,EPOLL_CTL_DEL, tmp->m_client->fd,0);
        printf("closing %d\n",tmp->m_client->fd);
        close(tmp->m_client->fd);
        http_conn::m_user_count--;
        
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}










