#include "stdafx.h"
#include "MyTask.h"
#include <assert.h>
#include <time.h>

#ifdef _DEBUG
#define OUTPUT TRACE
#else
#define OUTPUT
#endif

SOCKET CMyTask::m_Socket = INVALID_SOCKET;

char CMyTask::m_strIp[] = { 0 };

/**
* @brief 构造一个任务
* @param[in] id 任务编号
*/
CMyTask::CMyTask(const char *ip, int port, CSimpleSocket *pClient, int id):CTask(id)
{
	strcpy_s(m_curIp, ip);
	m_nPort = port;
	m_pClient = pClient;
}

/// 默认析构函数
CMyTask::~CMyTask(void)
{
}

/// delete this
void CMyTask::Destroy()
{
	delete this;
}

/// 获取本机IP
const char* getLocalHost()
{
	static char localhost[64] = { "127.0.0.1" };
	char hostname[128] = { 0 };
	if (0 == gethostname(hostname, 128))
	{
		hostent *host = gethostbyname(hostname);
		// 将ip转换为字符串
		char *hostip = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
		memcpy(localhost, hostip, strlen(hostip));
	}
	return localhost;
}

/// CMyTask重载的任务执行函数
void CMyTask::taskProc()
{
	if (INVALID_SOCKET == CMyTask::GetSocket())
	{
		clock_t t = clock();
		if (0 == m_pClient->connectServer(m_curIp, m_nPort))
		{
			char buffer[512] = { 0 };
			static const char *localhost = getLocalHost();
			GetRegisterPkg(buffer, localhost, m_curIp);
			m_pClient->sendData(buffer, strlen(buffer));
			memset(buffer, 0, 64);
			if (m_pClient->recvData(buffer, 63) >= 0 && 0 == strcmp("register:success", buffer))
			{
				strcpy_s(CMyTask::m_strIp, m_curIp);
				CMyTask::SetSocket(m_pClient->getSocket());
			}
		}
		SOCKET s = m_pClient->GetSocket();
		m_pClient->Close();
		OUTPUT("======> Socket [%d] Connect %s:%d use time = %d\n", s, m_curIp, m_nPort, clock() - t);
	}
}
