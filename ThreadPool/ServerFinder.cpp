#include "ServerFinder.h"

int CServerFinder::m_nWaitTime = 10;

// 寻找开启指定端口的服务器
// 最佳线程数：https://www.cnblogs.com/bobsha/p/6178995.html
void CServerFinder::FindServer(void *param)
{
	CServerFinder *pThis = (CServerFinder*)param;
	pThis->m_nState = Is_Finding;
	CMyThreadPool *pool = pThis->pool;
	while (pThis->m_nState == Is_Finding)
	{
		if(pThis->m_bWait || CMyTask::isFind())
		{
			Sleep(50);
			continue;
		}
		clock_t tm = clock();
		int k = 0;
		for (int i = 255; i ; --i)
		{
			char ip[64];
			sprintf_s(ip, "%s%d", pThis->m_strIp, i);
			int n = i % CLIENT_NUM;
			pool->addTask(new CMyTask(ip, pThis->m_nPort, pThis->m_pClient[n], ++k));
			if (0 == n)
				pool->Wait(10);
			if (pThis->m_bWait || CMyTask::isFind())
				break;
		}
		pool->Wait(10);
		tm = clock() - tm;
		TRACE("======> Socket = %d, use %.3fs\n", CMyTask::GetSocket(), tm/1000.0);
		int n = (pThis->m_nWaitTime * 1000) / 50;
		for (int k = IS_NIGHT() ? 2*n : n; pThis->m_nState == Is_Finding && k; --k)
			Sleep(50);
	}
	pThis->m_nState = End_Finding;
}


CServerFinder::CServerFinder(const char *ip, int port, int n)
{
	m_bWait = false;
	strcpy_s(m_strIp, ip);
	m_nPort = port;
	m_nState = Not_Finding;
	for (int i = 0; i < CLIENT_NUM; ++i)
		m_pClient[i] = new CSimpleSocket();
	pool = new CMyThreadPool(n);
	_beginthread(&FindServer, 0, this);
}


CServerFinder::~CServerFinder()
{
	Exit();
	pool->Wait(10);
	pool->destroyThreadPool();
	delete pool;
	for (int i = 0; i < CLIENT_NUM; ++i)
		delete m_pClient[i];
}
