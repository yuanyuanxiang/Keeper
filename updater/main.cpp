#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <io.h>
#include <direct.h>

#define POSTFIX "_update.exe" //待升级程序的后缀名称

// 退出时打印信息
#ifdef _DEBUG
#define MY_LOG(pFile, pSrc, n) { OutputDebugStringA("======> "); OutputDebugStringA(pSrc); \
	if (pFile){ fwrite(pSrc, strlen(pSrc), 1, pFile); fclose(pFile); } return n; }
#else
#define MY_LOG(pFile, pSrc, n) { if (pFile){ fwrite(pSrc, strlen(pSrc), 1, pFile); fclose(pFile); } return n; }
#endif

#define LOG(pFile, pSrc) MY_LOG(pFile, pSrc, -1)
#define LOG_OK(pFile, pSrc) MY_LOG(pFile, pSrc, 0)
#define WAIT_TIME 10 //等待时间（分钟）

// 目录访问器[成员易变的]
struct Folder
{
	char buf[_MAX_PATH], *pos;
	Folder()
	{
		// 获取程序当前目录
		GetModuleFileNameA(NULL, buf, sizeof(buf));
		pos = buf;
		while ('\0' != *pos) ++pos;
		while ('\\' != *pos) --pos;
		++pos;
	}
	// 获取当前目录下的文件或目录
	inline const char* get(const char *file)
	{
		strcpy(pos, file);
		return buf;
	}
	// 获取子目录下的文件或目录
	inline const char* get(const char *child, const char *file)
	{
		sprintf(pos, "%s\\%s", child, file);
		return buf;
	}
};

// argv[1]为升级程序的名称(不含后缀)
// 检测当前目录是否存在argv[1]_update.exe的文件
// 如果存在，则删除argv[1].exe，用上述文件替换
int main(int argc, char ** argv)
{
	Folder folder; // 当前目录及初始化
	FILE *f = fopen(folder.get("updater.log"), "a+");
	if (f)
	{
		time_t timep = time(NULL);
		char tmp[64];
		strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
		char log[600];
		sprintf_s(log, "%s 参数%d个: argv[0] = %s, argv[1] = %s, arg[2] = %s\n", 
			tmp, argc, argv[0], argc>1 ? argv[1]:"null", argc>2 ? argv[2]:"null");
		fwrite(log, strlen(log), 1, f);
	}else 
		return 0xDEAD;
	if (1 == argc)
		LOG(f, "参数只有一个.\n");

	char id[128];// 读取Keeper.conf "id" 字段
	GetPrivateProfileStringA("module", "id", "", id, sizeof(id), folder.get("keep.conf"));
	if (id[0] == 0)
		LOG(f, "读取配置文件失败.\n");

	const char *name = argv[1];// 升级文件
	char update[128];// 升级临时文件
	sprintf_s(update, "%s%s", name, POSTFIX);
	if (-1 == _access(folder.get(update), 0))
		LOG(f, "升级文件不存在.\n");
	char buf[128], exe[_MAX_PATH];
	sprintf(buf, "%s.exe", name);
	strcpy_s(exe, folder.get(buf));

	bool isKeeper = 0 == strcmp(name, "Keeper");// 是否升级守护程序
	if (isKeeper)
	{
		// 借助互斥量，等待守护程序
		HANDLE m = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, id);
		if (m && WAIT_TIMEOUT == WaitForSingleObject(m, WAIT_TIME*60000))
			LOG(f, "旧版程序仍在使用中.\n");
		CloseHandle(m);
	}
	if (-1 == _access(folder.get(".old"), 0))
		_mkdir(folder.get(".old"));
	if (0 == _access(folder.get(".old", buf), 0))
		DeleteFileA(folder.get(".old", buf));
	CopyFileA(exe, folder.get(".old", buf), FALSE);
	for (int k = WAIT_TIME*60; FALSE == DeleteFileA(exe) && k; --k)
		Sleep(1000);

	if ( 0 == rename(folder.get(update), exe) )
	{
		if (isKeeper)
		{
			SHELLEXECUTEINFO ShExecInfo = { 0 };
			ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
			ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
			ShExecInfo.lpFile = exe;
			ShExecInfo.lpParameters = argc > 2 ? argv[2] : NULL;
			if (ShellExecuteEx(&ShExecInfo))
				LOG_OK(f, "升级守护程序成功.\n");
			LOG(f, "启动守护程序失败.\n");
		}else 
			LOG_OK(f, "升级程序成功.\n");
	}
	else
		LOG(f, "待升级程序被占用.\n");
}
