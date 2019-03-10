#pragma once

#include <time.h>

/** 
* @class tick
* @brief 毫秒表计时器[8 bytes]
*/
class tick
{
protected:
	/// 开始时间
	clock_t start;
	/// 当前时间
	inline clock_t now() const { return clock(); }
public:
	/// 构造并计时
	tick() : start(now()){ }
	/// 开始计时
	inline void begin() { start = now(); }
	/// 重新计时
	inline void reset() { start = now(); }
	/// 计时（ms）
	inline clock_t time() const { return (now() - start); }
};

/** 
* @class tick_s
* @brief 秒表计时器[8 bytes]
*/
class tick_s
{
protected:
	/// 开始时间
	time_t start;
	/// 当前时间
	inline time_t now() const { return ::time(NULL); }
public:
	/// 构造并计时
	tick_s() : start(now()){ }
	/// 开始计时
	inline void begin() { start = now(); }
	/// 重新计时
	inline void reset() { start = now(); }
	/// 设置启动时间
	inline void set_beginTime(int nBegin) { start = nBegin; }
	/// 计时（s）
	inline time_t time() const { return (now() - start); }
};
