#include "con_pool.h"

 connection_pool * connection_pool::GetInstance(){
    static connection_pool con_pool;
    return &con_pool;
 }

 void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn){
     m_url =url;			 
	 m_Port	=Port;	
	 m_User	=User;
	 m_PassWord	=PassWord;
	 m_DatabaseName =DataBaseName;

     for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			std::cout<<"mysql init wrong!"<<std::endl;
            exit(1);
		}

		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			std::cout<<"MySQL connect Error"<<std::endl;
			exit(1);
		}
		connList.push_back(con);
		++m_FreeConn;
	}

    reserve = sem(m_FreeConn);
	m_MaxConn = m_FreeConn;
 }

MYSQL * connection_pool::GetConnection(){
    reserve.wait();
    lock.lock();
	// printf("getconn,size:  %d\n",connList.size());
    MYSQL* temp=connList.front();
	// printf("为null? %d\n",temp==NULL);
    connList.pop_front();
    m_CurConn++;
    m_FreeConn--;
    lock.unlock();
	// printf("getconn done,size:  %d\n",connList.size());
    return temp;
}

void connection_pool::ReleaseConnection(MYSQL *conn){
    lock.lock();
	// printf("push back为null? %d\n",conn==NULL);
    connList.push_back(conn);
    m_CurConn--;
    m_FreeConn++;
    lock.unlock();
    reserve.post();

	//printf("releaseConn done,size: %d\n",connList.size());
}

int connection_pool::GetFreeConn(){
    return m_FreeConn;
}

connection_pool::~connection_pool(){
    if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);
		}
		connList.clear();
	}
}







