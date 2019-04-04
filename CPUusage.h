#include <Windows.h>

#ifdef _M_IX86
// XP:无法定位程序输入点 K32GetProcessMemoryInfo
// @see https://blog.csdn.net/caimouse/article/details/48676285
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif
#pragma  comment(lib,"Psapi.lib")
#endif

#include <psapi.h>


// 获取物理处理器个数的代码来源：
// GetLogicalProcessorInformation function
// https://msdn.microsoft.com/en-us/library/ms683194(v=vs.85).aspx
typedef BOOL (WINAPI *LPFN_GLPI)(
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
	PDWORD);

// Helper function to count set bits in the processor mask.
DWORD CountSetBits(ULONG_PTR bitMask);


//原理：调用GetProcessTimes()，并与上次调用得到的结果相减，即得到某段时间内CPU的使用时间
//C++ 获取特定进程规定CPU使用率  原文：http://blog.csdn.net/liuqx97bb/article/details/52058657
class CPUusage
{ 
private:
	typedef long long int64_t;
	typedef unsigned long long uint64_t;
	HANDLE _hProcess;
	int _processor;//cpu数量  
	int64_t _last_time;     //上一次的时间  
	int64_t _last_system_time;


	// 时间转换  
	uint64_t file_time_2_clockT(const FILETIME* ftime) const
	{
		ULARGE_INTEGER ui;
		ui.LowPart = ftime->dwLowDateTime;
		ui.HighPart = ftime->dwHighDateTime;
		return (ui.QuadPart - 116444736000000000) / 10; // us
	}

	// 获得CPU的核数  
	int get_processor_number() const;

	//初始化
	void init()
	{
		_last_system_time = 0;
		_last_time = 0;
		_hProcess = 0;
		_processor = get_processor_number();
	}

	//关闭进程句柄
	void clear()
	{
		if (_hProcess) {
			CloseHandle(_hProcess);
			_hProcess = 0;
		}
	}

public:
	CPUusage() { init(); }
	~CPUusage() { clear(); }

	//返回值为进程句柄，可判断OpenProcess是否成功
	HANDLE setpid(DWORD ProcessID) { 
		clear();//如果之前监视过另一个进程，就先关闭它的句柄
		init(); 
		return _hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ProcessID); 
	}

	//-1 即为失败或进程已退出； 如果成功，首次调用会返回-2（中途用setpid更改了PID后首次调用也会返回-2）
	float get_cpu_usage();

	// 获取进程内存占用（Mb）
	float get_mem_usage()
	{
		PROCESS_MEMORY_COUNTERS pmc;
		return _hProcess ? ( GetProcessMemoryInfo(_hProcess, &pmc, sizeof(pmc)) 
			? pmc.WorkingSetSize / (1024.f*1024.f) : 0 ) : 0;
	}

	// 获取进程的句柄数量
	int get_handles_count()
	{
		DWORD count = 0;
		if (_hProcess)
			GetProcessHandleCount(_hProcess, &count);
		return count;
	}
};
