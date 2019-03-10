#pragma once
#include "ThreadPool/MyThreadPool.h"
#include "ThreadPool/MyTask.h"
#include <time.h>
#include <process.h>

// 当前时间是否是晚上
inline bool IS_NIGHT()
{
	int t = time(0) % 86400 + 28800 ;
	return t < 21600 || t > 64800; // t属于[0-6) 或 (18-24]
}

// FindServer 线程状态
enum FindState
{
	Is_Finding = 0,		// 正在执行Find
	Not_Finding,		// 停止find
	End_Finding,		// 结束find
};

#define CLIENT_NUM 8

// 寻找指定网段打开了指定端口的设备IP
class CServerFinder
{
private:
	bool m_bWait; // 是否处于等待状态
	FindState m_nState; // find状态
	char m_strIp[64];
	int m_nPort;
	CSimpleSocket *m_pClient[CLIENT_NUM]; // Socket 封装类的对象数组
	CMyThreadPool *pool;
	static int m_nWaitTime;// 等待间隔（单位：s，默认10s）
	static void FindServer(void *param);

public:
	void Exit() { if (Is_Finding == m_nState) { m_nState = Not_Finding; while(End_Finding != m_nState) Sleep(10); } }

	void SetIpPort(const char *ip, int port) { m_bWait = false; strcpy_s(m_strIp, ip); m_nPort = port; }

	void SetThreadWait() { m_bWait = true; }

	bool IsReady() const { return !m_bWait; }

	static void SetWaitTime(int n) { m_nWaitTime = n; }

	CServerFinder(const char *ip, int port, int n);

	~CServerFinder();
};
