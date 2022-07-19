#include "http.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;


 int http_conn::m_TRIGMode=0;
 char* http_conn::doc_root=NULL;
 int http_conn:: m_epollfd=0;
 int http_conn::m_user_count=0;


void setnonblocking(int fd){
    int old_op=fcntl(fd,F_GETFL);
    int new_op=old_op|O_NONBLOCK;
    fcntl(fd, F_SETFL, new_op);
}

void addfd(int epollfd,int fd,int trig_md){
    epoll_event event;
    event.data.fd=fd;

    if(trig_md==1){
        event.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    }
    else{
        event.events=EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd=fd;

    if(TRIGMode==1){
        event.events=ev | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    }
    else{
        event.events=ev | EPOLLRDHUP | EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epollfd,int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}





void http_conn::initmysql_result(connection_pool *connPool){
     
    MYSQL *mysql = connPool->GetConnection();

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        std::cout<<"mysql query wrong in initmysql_result!"<<std::endl;
        exit(1);
    }

    MYSQL_RES *result = mysql_store_result(mysql);

    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
    connPool->ReleaseConnection(mysql);
}

void http_conn::close_conn()
{
    if (m_sockfd != -1)
    {
        // printf("1closing %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}



void http_conn::init(int sockfd, const sockaddr_in &addr){
    m_sockfd=sockfd;
    m_address=addr;
    addfd(m_epollfd,m_sockfd,m_TRIGMode);
    m_user_count++;

    init();

}

void http_conn::init(){
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    
    m_content_length = 0;
    
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    
    m_string=NULL;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

void http_conn::process(){
    
    // printf("%d\n",m_sockfd);
    HTTP_CODE read_ret = process_read();
    
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);

}

http_conn::HTTP_CODE http_conn::process_read(){
    LINE_STATUS line_status=LINE_OK;
    m_check_state=CHECK_STATE_REQUESTLINE;
    m_start_line=m_checked_idx;

    while((m_check_state==CHECK_STATE_CONTENT&&line_status==LINE_OK)||((line_status=parse_line())==LINE_OK)){
        // printf("m_check_state:    %d\n",m_check_state);
       
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            if(BAD_REQUEST==parse_request_line(m_read_buf+m_start_line))
                return BAD_REQUEST;
            break;
        case CHECK_STATE_HEADER:
            if(GET_REQUEST==parse_headers(m_read_buf+m_start_line))
                return do_request();
            break;
        case  CHECK_STATE_CONTENT:
            if(GET_REQUEST==parse_content(m_read_buf+m_start_line))
                return do_request();
            line_status=LINE_OPEN;
            break;
        
        default:
            std::cout<<"wrong check_state!"<<std::endl;
            return INTERNAL_ERROR;
            break;
        }

        m_start_line=m_checked_idx;
    }

    return NO_REQUEST;
}

bool http_conn::process_write(http_conn::HTTP_CODE ret){

    //  printf("%d\n",ret);
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

bool http_conn::read_once(){
    if(m_read_idx>=READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read=0;
    if(m_TRIGMode==0){
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        if(bytes_read<=0){
            return false;
        }
        m_read_idx+=bytes_read;
        return true;
    }
    else{
        while(true){
            bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }

        return true;
    }

}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write(){
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                // printf("LINGER\n");
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}


http_conn::LINE_STATUS http_conn::parse_line(){
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        
        if (m_read_buf[m_checked_idx] == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (m_read_buf[m_checked_idx] == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char *text){

    
    //  printf("%s\n",text);
    char* temp;
    m_url=strpbrk(text," \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }

    // printf("a");
    *m_url++='\0';
    if(strcasecmp(text, "GET") == 0) m_method=GET;
    else if(strcasecmp(text, "POST") == 0) m_method=POST;
    else {return BAD_REQUEST;}
    // printf("b");
    m_url += strspn(m_url, " \t");


    temp=strpbrk(m_url," \t");
    if (!temp)
    {
        return BAD_REQUEST;
    }
    // printf("c");
    *temp++='\0';
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    // printf("d");
    temp += strspn(temp, " \t");

    if (strcasecmp(temp, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    // printf("e");

    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;

}
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else
    {
        // printf("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::parse_content(char *text){
    if(m_read_idx>=m_checked_idx+m_content_length){
        // printf("okk\n");
        text[m_content_length]='\0';
        m_string=text;
        return GET_REQUEST;
    }
    // printf("not okk\n");
    return NO_REQUEST;
}


http_conn::HTTP_CODE http_conn::do_request(){
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);  

    if(m_method==POST&&(*(m_url + 1) == '2' || *(m_url + 1) == '3')){

        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';


        if (*(m_url+ 1) == '3')
        {

            if(users.find(name) != users.end()){
                strncpy(m_real_file + len, "/registerError.html", FILENAME_LEN - len - 1);
            }
            else{
                 //printf("okk1\n");
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, name);
                strcat(sql_insert, "', '");
                strcat(sql_insert, password);
                strcat(sql_insert, "')");

                m_lock.lock();
                // printf("okk1\n");
                // printf("%d\n",mysql==NULL);
                int res = mysql_query(mysql, sql_insert);
                // printf("okk2\n");
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();


                // printf("okk2\n");
                if (!res)
                    strncpy(m_real_file + len, "/log.html", FILENAME_LEN - len - 1);
                else
                    strncpy(m_real_file + len, "/registerError.html", FILENAME_LEN - len - 1);

                free(sql_insert);
            }       
        }

        else if (*(m_url + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strncpy(m_real_file + len, "/welcome.html", FILENAME_LEN - len - 1);
            else
                strncpy(m_real_file + len, "/logError.html", FILENAME_LEN - len - 1);
        }
    }
    else if (*(m_url+ 1) == '0')
    {  
        strncpy(m_real_file + len, "/register.html", FILENAME_LEN - len - 1);
    }
    else if (*(m_url+ 1) == '1')
    {
        strncpy(m_real_file + len, "/log.html", FILENAME_LEN - len - 1);
    }
    else if (*(m_url + 1) == '5')
    {
        strncpy(m_real_file + len, "/picture.html", FILENAME_LEN - len - 1);
    }
    else if (*(m_url + 1) == '6')
    {
        strncpy(m_real_file + len, "/video.html", FILENAME_LEN - len - 1);
    }
    else if (*(m_url + 1) == '7')
    {
        strncpy(m_real_file + len, "/fans.html", FILENAME_LEN - len - 1);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // printf("m_real_file:   %s/n",m_real_file);

    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}


bool http_conn::add_response(const char *format, ...){
   if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true; 
}
bool http_conn::add_content(const char *content){
    return add_response("%s", content);
}
bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_length){
    return add_response("Content-length:%d\r\n",content_length) &&
    add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close") &&
    add_response("%s", "\r\n");
}












