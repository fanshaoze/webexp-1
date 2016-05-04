#include "stdafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>

using namespace std;

#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口
//Http 重要头部数据
struct HttpHeader{

    char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
    char url[1024]; // 请求的 url
    char host[1024]; // 目标主机
    char cookie[1024 * 10]; //cookie
    HttpHeader(){
        ZeroMemory(this,sizeof(HttpHeader));
		//ZeroMemory是美国微软公司的软件开发包SDK中的一个宏。 其作用是用0来填充一块内存区域。
    }
};
BOOL InitSocket();
void ParseHttpHead(char *buffer,HttpHeader * httpHeader);//解析头部
BOOL ConnectToServer(SOCKET *serverSocket,char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
//LPVOID,是一个没有类型的指针，也就是说你可以将任意类型的指针赋值给LPVOID类型的变量（一般作为参数传递），然后在使用的时候再转换回来。
/*
__stdcall是函数调用约定的一种，函数调用约定主要约束了两件事：
1.参数传递顺序
2.调用堆栈由谁（调用函数或被调用函数）清理
常见的函数调用约定：stdcall cdecl fastcall thiscall naked call
__stdcall表示
1.参数从右向左压入堆栈
2.函数被调用者修改堆栈
3.函数名(在编译器这个层次)自动加前导的下划线，后面紧跟一个@符号，其后紧跟着参数的尺寸
在win32应用程序里,宏APIENTRY，WINAPI，都表示_stdcall，非常常见。
*/
//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam{
SOCKET clientSocket;
SOCKET serverSocket;
};
int _tmain(int argc, _TCHAR* argv[])
{
    cout<<"代理服务器正在启动"<<endl;
    cout<<"初始化..."<<endl;
    if(!InitSocket()){
        cout<<"socket 初始化失败"<<endl;
        return -1;
    }
	cout<<"代理服务器正在运行，监听端口 :"<<ProxyPort<<endl;
    cout<<"代理服务器正在运行，监听端口 "<<ProxyPort<<endl;
    SOCKET acceptSocket = INVALID_SOCKET;
	/*
	#define INVALID_SOCKET  (SOCKET)(~0)
	(~0)的值为-1；
	(SOCKET)(~0)的值为：十六进制0xFFFFFFFF（十进制4294967295）。
	*/
    ProxyParam *lpProxyParam;
    HANDLE hThread;
    DWORD dwThreadID;
    //代理服务器不断监听
    while(true){
        acceptSocket = accept(ProxyServer,NULL,NULL);
		/*
		 服务程序调用accept函数从处于监听状态的流套接字sd的客户连接请求队列中取出
		排在最前的一个客户请求，并且创建一个新的套接字来与客户套接字创建连接通道
		仅用于TCP套接字
		仅用于服务器
		利用新创建的套接字（ newsock）与客户通信
		*/
        lpProxyParam = new ProxyParam;
        if(lpProxyParam == NULL){
            continue;
        }
        lpProxyParam->clientSocket = acceptSocket;
        hThread = (HANDLE)_beginthreadex(NULL, 0,&ProxyThread,(LPVOID)lpProxyParam, 0, 0);
        CloseHandle(hThread);
        Sleep(200);
    }
    closesocket(ProxyServer);
    WSACleanup();
    return 0;
}
//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket(){
    //加载套接字库（必须）
    WORD wVersionRequested;
    WSADATA wsaData;
	//WSADATA，一种分子结构。这个结构被用来存储被WSAStartup函数调用后返回的Windows Sockets数据

    //套接字加载时错误提示
    int err;
    //版本 2.2
    wVersionRequested = MAKEWORD(2, 2);
	//#define MAKEWORD(a, b)  ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
    //加载 dll 文件 Scoket 库
    err = WSAStartup(wVersionRequested, &wsaData);
    if(err != 0){
    //找不到 winsock.dll
        cout<<"加载 winsock 失败，错误代码为: "<<WSAGetLastError()<<endl;
        return FALSE;
    }
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) !=2)
    {
        cout<<"不能找到正确的 winsock 版本"<<endl;
        WSACleanup();
        return FALSE;
    }
    ProxyServer= socket(AF_INET, SOCK_STREAM, 0);
	//SOCK_STREAM 提供有序的、可靠的、双向的和基于连接的字节流，使用带外数据传送机制，为Internet地址族使用TCP。
	//SOCK_DGRAM 支持无连接的、不可靠的和使用固定大小（通常很小）缓冲区的数据报服务，为Internet地址族使用UDP。
	//SOCK_STREAM类型的套接口为全双向的字节流。对于流类套接口，在接收或发送数据前必需处于已连接状态
    if(INVALID_SOCKET == ProxyServer){
        cout<<"创建套接字失败，错误代码为"<<WSAGetLastError()<<endl;
        return FALSE;
    }
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(ProxyPort);
	//htons是将整型变量从主机字节顺序转变成网络字节顺序， 就是整数在地址空间存储方式变为：高位字节存放在内存的低地址处。
    ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	/*
	存放Ipv4的地址
	typedef struct in_addr {
        union {
                struct { UCHAR s_b1,s_b2,s_b3,s_b4; } S_un_b;
                struct { USHORT s_w1,s_w2; } S_un_w;
                ULONG S_addr;
        } S_un;
		*/
	//in_addr是一个函数，可以用来表示一个32位的IPv4地址
    if(bind(ProxyServer,(SOCKADDR*)&ProxyServerAddr,sizeof(SOCKADDR)) == SOCKET_ERROR){
		//bind理解为给socket结构里面的Ip地址和端口号赋值
		cout<<"绑定套接字失败"<<endl;
		return FALSE;
    }
    if(listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR){
		cout<<"监听端口"<<ProxyPort<< "失败"<<endl;
		return FALSE;
    }
    return TRUE;
}
//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter)
{
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer,MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket,Buffer,MAXSIZE,0);
	if(recvSize <= 0){
		goto error;
	}
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer,recvSize + 1);
	memcpy(CacheBuffer,Buffer,recvSize);
	ParseHttpHead(CacheBuffer,httpHeader);//解析头部，把头部的需要用的四种数据都放入到httpheader中
	delete CacheBuffer;
	if(!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket,httpHeader->host)) {
		goto error;
	}
	cout<<"代理连接主机 "<<httpHeader->host<<" 成功"<<endl;
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	ret = send(((ProxyParam *)lpParameter)->serverSocket,Buffer,strlen(Buffer)+ 1,0);
	/*send():
	向一个已连接的套接口发送数据。
	#include <winsock.h>
	int PASCAL FAR send( SOCKET s, const char FAR* buf, int len, int flags);
	s：一个用于标识已连接套接口的描述字。
	buf：包含待发送数据的缓冲区。
	len：缓冲区中数据的长度。
	flags：调用执行方式。*/
	//等待目标服务器返回数据
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket,Buffer,MAXSIZE,0);
	//函数原型int recv( _In_ SOCKET s, _Out_ char *buf, _In_ int len, _In_ int flags);
	if(recvSize <= 0){
		goto error;
	}
	//将目标服务器返回的数据直接转发给客户端
	ret = send(((ProxyParam*)lpParameter)->clientSocket,Buffer,sizeof(Buffer),0);
	//错误处理
	error:
	cout<<"关闭套接字"<<endl;
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	_endthreadex(0);
	return 0;
	}
//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer,HttpHeader * httpHeader)
{
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer,delim,&ptr);//提取第一行
	cout<<p<<endl;
	if(p[0] == 'G')
	{//GET 方式
		memcpy(httpHeader->method,"GET",3);
		memcpy(httpHeader->url,&p[4],strlen(p) -13);//13.14是从哪里来的，如何计算的
		//cout<<"%s\n",httpHeader->url);
	}
	else if(p[0] == 'P')
	{//POST 方式
		memcpy(httpHeader->method,"POST",4);
		memcpy(httpHeader->url,&p[5],strlen(p) - 14);
	}
	cout<<httpHeader->url<<endl;
	//p = strtok_s(NULL,delim,&ptr);
	
	while(p)
	{//具体内容看书，关于cookie，请求报文的头部的结构
		//cout<<"%s\n",p);
		switch(p[0]){
			case 'H'://Host
			memcpy(httpHeader->host,&p[6],strlen(p) - 6);
			break;
			case 'C'://Cookie
			if(strlen(p) > 8)
			{
				char header[8];
				ZeroMemory(header,sizeof(header));
				memcpy(header,p,6);
				if(!strcmp(header,"Cookie"))
				{
					memcpy(httpHeader->cookie,&p[8],strlen(p) -8);
				}
			}
			break;
			default:
			break;
		}
		p = strtok_s(NULL,delim,&ptr);
	}
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket,char *host)
{
	/*
	struct sockaddr_in {
        short   sin_family;
        u_short sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
	};

	struct  hostent {
        char    FAR * h_name;           // official name of host
        char    FAR * FAR * h_aliases;  // alias list 
        short   h_addrtype;             // host address type 
        short   h_length;               // length of address 
        char    FAR * FAR * h_addr_list; // list of addresses 
	#define h_addr  h_addr_list[0]          // address, for backward compat 
	};

	*/
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	//htons是将整型变量从主机字节顺序转变成网络字节顺序， 就是整数在地址空间存储方式变为：高位字节存放在内存的低地址处。
	HOSTENT *hostent = gethostbyname(host);
	//hostent是host entry的缩写，该结构记录主机的信息，包括主机名、别名、地址类型、
	//地址长度和地址列表。之所以主机的地址是一个列表的形式，原因是当一个主机有多个网络接口时，自然有多个地址。
	//gethostbyname()返回对应于给定主机名的包含主机名字和地址信息的hostent结构指针。结构的声明与gethostbyaddr()中一致。
	if(!hostent)
	{
		return FALSE;
	}
	in_addr Inaddr=*( (in_addr*) *hostent->h_addr_list);

	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	/*
	in_addr_t inet_addr(const char* strptr);
	返回：若字符串有效则将字符串转换为32位二进制网络字节序的IPV4地址，否则为INADDR_NONE
	struct in_addr{
		in_addr_t s_addr;
	}
	*/

	/*
		char*inet_ntoa(struct in_addr in);
		将一个十进制网络字节序转换为点分十进制IP格式的字符串。
		一个网络上的IP地址
		返回值：
		如果正确，返回一个字符指针，指向一块存储着点分格式IP地址的静态缓冲区（同一线程内共享此内存）；错误，返回NULL。
	*/
	*serverSocket = socket(AF_INET,SOCK_STREAM,0);
	if(*serverSocket == INVALID_SOCKET)
	{
		return FALSE;
	}
	if(connect(*serverSocket,(SOCKADDR *)&serverAddr,sizeof(serverAddr))== SOCKET_ERROR)
	{
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}
