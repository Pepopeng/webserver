#ifndef _CON_POOL_
#define _CON_POOL_

#include <mysql/mysql.h>
#include <string.h>
#include <string>
#include "../lock/lock.h"
#include <list>
using namespace std;

class connection_pool
{
public:
	MYSQL *GetConnection();				 
	void ReleaseConnection(MYSQL *conn); 
	int GetFreeConn();					 			

	static connection_pool *GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn); 

private:
	connection_pool(){};
	~connection_pool();

	int m_MaxConn;  
	int m_CurConn;  
	int m_FreeConn; 
	locker lock;
	list<MYSQL *> connList; 
	sem reserve;

public:
	string m_url;			 
	string m_Port;		
	string m_User;		 
	string m_PassWord;	
	string m_DatabaseName; 
};



#endif
