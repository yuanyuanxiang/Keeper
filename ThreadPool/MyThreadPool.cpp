#include "stdafx.h"
#include "MyThreadPool.h"
#include "Task.h"
#include<cassert>
#include<iostream>


/**
* @brief 构造一个容量确定的线程池
* @param[in] nDefaultSize 线程池默认容量
*/
CMyThreadPool::CMyThreadPool(int nDefaultSize)
{
	m_nThreadNum = nDefaultSize;
	m_bIsExit = false;
	for(int i = 0; i < nDefaultSize; i++)
	{
		CMyThread *p = new CMyThread(this);
		m_IdleThreadStack.push(p);
		p->startThread();
	}
}

/// 默认析构函数
CMyThreadPool::~CMyThreadPool(void)
{
}

/**
* @brief 改变线程池的大小
* @return nSize 线程池大小
*/
void CMyThreadPool::ChangeSize(int nSize)
{
	m_mutex.Lock();
	if (nSize > m_nThreadNum)
	{
		while (nSize > m_nThreadNum) AddThread();
	}
	else if (nSize < m_nThreadNum)
	{
		while (nSize < m_nThreadNum) SubtractThread();
	}
	m_mutex.Unlock();
}

/** 
* @brief 将线程从活动队列取出，放入空闲线程栈中.在取之前判断此时任务队列是否有任务.
* 如任务队列为空时才挂起,否则从任务队列取任务继续执行.
*/
bool CMyThreadPool::SwitchActiveThread( CMyThread *pThread)
{
	CTask *pTask = NULL;
	m_mutex.Lock();
	if(pTask = m_TaskQueue.pop())//任务队列不为空,继续取任务执行
	{
		pThread->assignTask(pTask);
		pThread->startTask();
	}
	else//任务队列为空，该线程挂起
	{
		m_ActiveThreadList.removeThread(pThread);
		m_IdleThreadStack.push(pThread);
	}
	m_mutex.Unlock();
	return true;
}

/**
* @brief 添加任务
* @param[in] *t 任务(指针)
* @param[in] priority 优先级,高优先级的任务将被插入到队首.
*/
void CMyThreadPool::addTask( CTask *t, PRIORITY priority )
{
	assert(t);
	if(!t || m_bIsExit)
		return;
	CTask *task = NULL;
	m_mutex.Lock();
	if(priority == PRIORITY::NORMAL)
	{
		m_TaskQueue.push(t);//进入任务队列
	}
	else if(PRIORITY::HIGH)
	{
		m_TaskQueue.pushFront(t);//高优先级任务
	}

	if(!m_IdleThreadStack.isEmpty())//存在空闲线程,调用空闲线程处理任务
	{
		if(task = m_TaskQueue.pop())//取出列头任务
		{
			CMyThread *pThread = m_IdleThreadStack.pop();
			m_ActiveThreadList.addThread(pThread);
			pThread->assignTask(task);
			pThread->startTask();
		}
	}
	m_mutex.Unlock();
}

/**
* @brief 销毁线程池
* @details clear m_TaskQueue，m_ActiveThreadList，m_IdleThreadStack
*/
void CMyThreadPool::destroyThreadPool()
{
	m_mutex.Lock();
	m_bIsExit = true;
	m_TaskQueue.clear();
	m_ActiveThreadList.clear();
	m_IdleThreadStack.clear();
	m_mutex.Unlock();
}
