#include "webserver.h"
#include <unistd.h>

int main(int argc,char* argv[]){

    string user = "root";
    string passwd = "123456";
    string databasename = "yourdb";


    int PORT=9006;

    int TRIGMode=0;
  
    int OPT_LINGER=0;
  
    int sql_num=8;

    int thread_num=8;


    int opt;
    const char *str = "p:m:o:s:t:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }

    webserver m_webserver;

    m_webserver.init(PORT, user, passwd,databasename,OPT_LINGER,TRIGMode,sql_num,thread_num);

    m_webserver.eventListen();

    m_webserver.eventLoop();

    return 0;
}