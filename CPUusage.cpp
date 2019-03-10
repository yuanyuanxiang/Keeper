#include "stdafx.h"
#include "CPUusage.h"


#define OUTPUT TRACE


float CPUusage::get_cpu_usage()
{
	if (!_hProcess)
		return 0;
	
	// 判断进程是否已经退出
	DWORD exitcode;
	GetExitCodeProcess(_hProcess, &exitcode);  
	if (exitcode != STILL_ACTIVE)
	{
		clear();
		return 0;
	}

	FILETIME creation_time;
	FILETIME exit_time;
	FILETIME kernel_time;
	FILETIME user_time;
	// 计算占用CPU的百分比
	if (!GetProcessTimes(_hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
	{
		clear();
		return 0;
	}
	// 内核时间
	int64_t system_time = (file_time_2_clockT(&kernel_time) + file_time_2_clockT(&user_time)) / _processor;

	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	int64_t time = file_time_2_clockT(&now);// 当前时间

	// 判断是否为首次计算
	if ((_last_system_time == 0) || (_last_time == 0))
	{
		_last_system_time = system_time;
		_last_time = time;
		return 0;
	}

	int64_t system_time_delta = system_time - _last_system_time;// 内核时间间隔
	int64_t time_delta = time - _last_time;// 时间间隔

	if (time_delta == 0) {
		return 0;
	}

	float cpu = system_time_delta * 100.f / time_delta;
	_last_system_time = system_time;
	_last_time = time;
	TRACE("======> CPU time: %.2f\n", cpu);

	return cpu;
}


DWORD CountSetBits(ULONG_PTR bitMask)
{
	DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
	DWORD i;

	for (i = 0; i <= LSHIFT; ++i)
	{
		bitSetCount += ((bitMask & bitTest)?1:0);
		bitTest/=2;
	}

	return bitSetCount;
}


// https://msdn.microsoft.com/en-us/library/ms683194(v=vs.85).aspx
int CPUusage::get_processor_number() const
{
	LPFN_GLPI glpi;
	BOOL done = FALSE;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
	DWORD returnLength = 0;
	DWORD logicalProcessorCount = 0;
	DWORD numaNodeCount = 0;
	DWORD processorCoreCount = 0;
	DWORD processorL1CacheCount = 0;
	DWORD processorL2CacheCount = 0;
	DWORD processorL3CacheCount = 0;
	DWORD processorPackageCount = 0;
	DWORD byteOffset = 0;
	PCACHE_DESCRIPTOR Cache;

	glpi = (LPFN_GLPI) GetProcAddress(GetModuleHandle(_T("kernel32")),
		"GetLogicalProcessorInformation");
	if (NULL == glpi) 
	{
		OUTPUT("\nGetLogicalProcessorInformation is not supported.\n");
		return 1;
	}

	while (!done)
	{
		DWORD rc = glpi(buffer, &returnLength);

		if (FALSE == rc) 
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
			{
				if (buffer) 
					free(buffer);

				buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
					returnLength);

				if (NULL == buffer) 
				{
					OUTPUT("\nError: Allocation failure\n");
					return 1;
				}
			} 
			else 
			{
				OUTPUT("\nError %d\n", GetLastError());
				return 1;
			}
		} 
		else
		{
			done = TRUE;
		}
	}

	ptr = buffer;

	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) 
	{
		switch (ptr->Relationship) 
		{
		case RelationNumaNode:
			// Non-NUMA systems report a single record of this type.
			numaNodeCount++;
			break;

		case RelationProcessorCore:
			processorCoreCount++;

			// A hyperthreaded core supplies more than one logical processor.
			logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
			break;

		case RelationCache:
			// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
			Cache = &ptr->Cache;
			if (Cache->Level == 1)
			{
				processorL1CacheCount++;
			}
			else if (Cache->Level == 2)
			{
				processorL2CacheCount++;
			}
			else if (Cache->Level == 3)
			{
				processorL3CacheCount++;
			}
			break;

		case RelationProcessorPackage:
			// Logical processors share a physical package.
			processorPackageCount++;
			break;

		default:
			OUTPUT("\nError: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n");
			break;
		}
		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		ptr++;
	}

	OUTPUT("\nGetLogicalProcessorInformation results:\n");
	OUTPUT("Number of NUMA nodes: %d\n", 
		numaNodeCount);
	OUTPUT("Number of physical processor packages: %d\n", 
		processorPackageCount);
	OUTPUT("Number of processor cores: %d\n", 
		processorCoreCount);
	OUTPUT("Number of logical processors: %d\n", 
		logicalProcessorCount);
	OUTPUT("Number of processor L1/L2/L3 caches: %d/%d/%d\n", 
		processorL1CacheCount,
		processorL2CacheCount,
		processorL3CacheCount);

	free(buffer);

	return processorCoreCount;
}
