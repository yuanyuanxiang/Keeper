/** 
* @file MyMutex.h
* @brief 互斥锁
*/

#pragma once

#include "../stdafx.h"

/** 
* @class CMyMutex 
* @brief 互斥锁封装了CRITICAL_SECTION对象的操作方法
*/
class CMyMutex
{
public:
	/// InitializeCriticalSection
	CMyMutex() { InitializeCriticalSection(&m_cs); }

	/// DeleteCriticalSection
	~CMyMutex() { DeleteCriticalSection(&m_cs); }

	/// EnterCriticalSection
	inline void Lock() { EnterCriticalSection(&m_cs); }

	/// LeaveCriticalSection
	inline void Unlock() { LeaveCriticalSection(&m_cs); }

private:
	/// 唯一的临界区成员
	CRITICAL_SECTION m_cs;
};
