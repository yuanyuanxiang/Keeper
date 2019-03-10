/** 
* @file MyThreadPool.h
* @brief 我的线程池
*/

#pragma once

#include "MyStack.h"
#include "MyList.h"
#include"MyQueue.h"
#include "MyThread.h"
#include "MyMutex.h"

class CTask;

/** 
* @enum PRIORITY 
* @brief 任务优先级 
*/
enum PRIORITY
{
	NORMAL,			/**< 正常优先级 */
	HIGH			/**< 高优先级 */
};

/** 
* @class CBaseThreadPool 
* @brief 线程池基类
*/
class CBaseThreadPool
{
public:
	/// 纯虚函数：切换活动线程
	virtual bool SwitchActiveThread(CMyThread *ptd) = 0;
};

/**
* @class CMyThreadPool 
* @brief 线程池
* @details 线程池包含三个要素：空闲线程栈、活动线程链表、任务队列
*/
class CMyThreadPool : public CBaseThreadPool
{
public:
	CMyThreadPool(int nDefaultSize);
	~CMyThreadPool(void);
	// 改变线程池的大小
	void ChangeSize(int nSize);

private:
	/// 切换活动线程
	virtual bool SwitchActiveThread(CMyThread* pThread);

public:
	/// 添加任务到线程池
	void addTask(CTask *t, PRIORITY priority = PRIORITY::NORMAL);
	/// 开始调度
	bool start() { return /* 当前不可用 */ 0; }
	/// 销毁线程池
	void destroyThreadPool();
	/// 获取活动线程个数
	inline int GetActiveThreadNum() { m_mutex.Lock(); int s = m_ActiveThreadList.getSize(); m_mutex.Unlock(); return s; }
	/// 获取线程池容量
	inline int GetThreadNum() { m_mutex.Lock(); int n = m_nThreadNum; m_mutex.Unlock(); return n; }
	/// 任务是否全部执行完毕
	inline bool IsTaskDone() { m_mutex.Lock(); bool b = m_TaskQueue.isEmpty(); m_mutex.Unlock(); return b; }
	/// 等待所有活动线程运行完毕[步长为mm]
	inline void Wait(int mm) { while (GetActiveThreadNum()) Sleep(mm); }

private:
	/// 线程池容量
	int m_nThreadNum;
	/// 退出的标记
	bool m_bIsExit;

	/// 任务锁(增加任务与切换任务时)
	CMyMutex m_mutex;

	/// 空闲线程栈(存放new出来的线程指针)
	CMyStack m_IdleThreadStack;
	/// 活动线程列表(存放new出来的线程指针)
	CMyList m_ActiveThreadList;
	/// 任务队列(存放new出来的任务指针)
	CMyQueue m_TaskQueue;

	/// 从线程池里减少一个线程（返回当前线程数）
	inline int SubtractThread()
	{
		Wait(10);
		CMyThread *pThread = m_IdleThreadStack.pop();
		if (pThread)
		{
			pThread->Exit();
			-- m_nThreadNum;
		}
		return m_nThreadNum;
	}

	/// 向线程池里添加一个线程（返回当前线程数）
	inline int AddThread()
	{
		Wait(10);
		CMyThread *pThread = new CMyThread(this);
		m_IdleThreadStack.push(pThread);
		pThread->startThread();
		++ m_nThreadNum;
		return m_nThreadNum;
	}
};
