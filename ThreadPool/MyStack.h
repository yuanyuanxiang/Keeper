/** 
* @file MyStack.h
* @brief 空闲线程栈
*/

#pragma once
#include<stack>
#include "MyThread.h"


/**
* @class CMyStack 
* @brief 空闲线程(指针)存放在栈上
*/
class CMyStack
{
public:
	/// 默认构造函数
	CMyStack() { }
	/// 默认析构函数
	~CMyStack() { }

	// 取出线程栈顶部的一个线程
	inline CMyThread* pop()
	{
		CMyThread *t = NULL;
		if(!m_stack.empty())
		{
			t = m_stack.top();
			m_stack.pop();
		}
		return t;
	}

	// 向线程栈添加一个线程
	inline void push(CMyThread *t) { assert(t); m_stack.push(t); }

	// 获取线程栈的大小
	inline int getSize() const { return m_stack.size(); }

	// 判断线程栈是否空
	inline bool isEmpty() const { return m_stack.empty(); }

	// 清空空闲线程栈
	inline void clear()
	{
		while(!m_stack.empty())
		{
			CMyThread *pThread = m_stack.top();
			m_stack.pop();
			pThread->Exit(); // 通知线程退出
		}
	}

private:
	/// 任务栈
	std::stack<CMyThread*> m_stack;
};
