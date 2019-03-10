/** 
* @file MyThread.h
* @brief 我的线程
*/

#pragma once
#include <cassert>
#include "../stdafx.h"

class CTask;
class CBaseThreadPool;

/**
* @class CMyThread 
* @brief 线程类
* @warning 只能从堆上创建线程
*/
class CMyThread
{
public:
	CMyThread(CBaseThreadPool *threadPool);

protected:
	~CMyThread();
	/// delete this
	inline void Delete() { delete this; }

public:
	/// 挂起线程
	inline void suspendThread() { ResetEvent(m_hEvent); }
	/// 有任务到来，通知线程继续执行
	inline void resumeThread() { SetEvent(m_hEvent); }
	/// 开始执行任务
	inline void startTask() { resumeThread(); }
	/// 通知回调函数退出（Exit之后会自动调用Delete）
	inline void Exit() { m_bIsExit = true; resumeThread(); }
	/// 将任务关联到线程类
	inline bool assignTask(CTask *pTask)
	{
		assert(pTask);
		m_pTask = pTask;
		return (NULL == pTask) ? false : true;
	}

	/// 开始线程
	inline bool startThread()
	{
		m_hThread = CreateThread(0, 0, threadProc, this, 0, &m_threadID);
		return (INVALID_HANDLE_VALUE == m_hThread) ? false : true;
	}

	/// 线程处理函数
	static DWORD WINAPI threadProc(LPVOID pParam);

	/// 线程ID
	DWORD m_threadID;
	/// 线程句柄
	HANDLE m_hThread;
	/// 发起退出
	bool m_bIsExit;

private:
	/// 信号
	HANDLE m_hEvent;
	/// 所执行任务
	CTask *m_pTask;
	/// 所属线程池
	CBaseThreadPool *m_pThreadPool;	
};
