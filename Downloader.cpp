#include "Downloader.h"
#include "md5driver.hpp"
#include <io.h>
#include <direct.h>

// 请求指令的固定长度
#define SIZE_1 100

// req					| resp
// (1) size:file		| size:bytes,md5
// (2) down:file		| ......

// 拼接请求指令
#define make_cmd(buf, cmd, arg) {memset(buf,0,SIZE_1); sprintf_s(buf,"%s:%s",cmd,arg);}

// 解析指令
#define parse_cmd(buf, arg) {arg=buf; while(*arg && ':'!=*arg)++arg; if(*arg)*arg++=0;}

// 打开指定文件准备写入
FILE* getFileId(const char *path)
{
	if (0 == _access(path, 0) && remove(path))
		return NULL;
	return fopen(path, "wb+");
}

// 检测文件是否已经下载过
bool Downloader::check(const char *file, int size, const char *MD5)
{
	int n = 0;// 文件大小
	FILE* f = fopen(file, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		n = ftell(f);
		fclose(f);
	}
	if (n != size)
		return false;
	// 比较Md5码
	char m[32+4] = {0};
	MDFile(file, m);
	return 0 == strcmp(m, MD5);
}

Downloader::Downloader(void) : m_bConnect(false), m_bCleanup(false), m_pSocket(new CBcecrSocket())
{
	memset(m_strApp, 0, sizeof(m_strApp));
}


Downloader::~Downloader(void)
{
	m_pSocket->unInit();
	delete m_pSocket;
	m_pSocket = NULL;
}


void Downloader::SetUpdateApp(const char *name)
{
	strcpy_s(m_strApp, name);
	m_bCleanup = false;
}


void Downloader::Connect(const char *ip, int port)
{
	if (false == m_bConnect && port)
	{
		m_bConnect = 0 == m_pSocket->init(ip, port, 1);
	}
}

void Downloader::Disconnect()
{
	m_bConnect = false;
	m_pSocket->unInit();
}

// 清理指定目录下的所有文件（除了"Keeper.exe"）
void ClearDir(const char *dir)
{
	std::string logDir(dir);
	//文件句柄
	intptr_t hFile = 0;
	//文件信息  
	struct _finddata_t fileinfo;
	std::string s;
	try
	{
		if ((hFile = _findfirst(s.assign(logDir).append("\\*.*").c_str(), &fileinfo)) != -1)
		{
			do{
				_strlwr(fileinfo.name);
				if (IS_DIR != fileinfo.attrib 
					&& strcmp(fileinfo.name, ".") && strcmp(fileinfo.name, "..")
					&& strcmp(fileinfo.name, "keeper.exe"))
				{
					std::string cur = s.assign(logDir).append("\\").append(fileinfo.name);
					DeleteFileA(cur.c_str());
				}
			} while (_findnext(hFile, &fileinfo) == 0);
			_findclose(hFile);
		}
	}catch (std::exception e){ if(hFile) _findclose(hFile); }
}

// 创建目标目录（如果中间目录不存在也将创建）
void MakeDir(const char *path)
{
	char buf[_MAX_PATH], *p = buf;
	strcpy_s(buf, path);
	for (;;)
	{
		while(*p != '\\' && *p) ++p;
		if (*p == 0)
			break;
		*p = 0;
		if (-1 == _access(buf, 0))
			_mkdir(buf);
		*p++ = '\\';
	}
}

bool Downloader::DownloadFile(const char *name, const char *postfix, BOOL isCommon, const char *type)
{
	char file[_MAX_PATH]; // 远端文件
	sprintf_s(file , "%s.%s", name, type);
	if (m_bConnect)
	{
		char temp[_MAX_PATH]; // 缓存文件
		sprintf_s(temp, "%s%s", name, postfix);
		char path[_MAX_PATH]; // 缓存文件完整路径
		strcpy_s(path, m_path.get(temp));
		char buf[SIZE_1]; // 远程文件完整路径
		const bool b32bit = 4 == sizeof(INT_PTR);
		sprintf_s(buf, "%s\\%s", m_strApp, file);
		char CMD[SIZE_1]; // 命令
		make_cmd(CMD, "size", buf);
		m_pSocket->sendData(CMD, SIZE_1);// 发文件名称
		m_pSocket->recvData(CMD, SIZE_1);// 收文件大小
		char *arg = 0;
		parse_cmd(CMD, arg);
		int size = atoi(arg);
		if (0 == size) // 服务器没有该文件
		{
			make_cmd(CMD, "down", "?");
			m_pSocket->sendData(CMD, SIZE_1);// 不下载
			char szLog[256];
			sprintf_s(szLog, "===> 服务器不存在文件: %s\n", buf);
			OutputDebugStringA(szLog);
			return false;
		}
		const char *Md5 = arg; //md5码
		while (*Md5 && ',' != *Md5)++Md5;
		if (*Md5) ++Md5;
		bool bNotExist = (-1 == _access(path, 0));// 本地文件不存在
		if (bNotExist || false == check(path, size, Md5))
		{
			const char *backup = m_path.get(".old"); // 创建".old"目录存放旧版本文件
			if (!m_bCleanup) { ClearDir(backup); m_bCleanup = true; }
			if(-1==isCommon && !bNotExist)
			{
				if (-1 == _access(backup, 0))
					_mkdir(backup);
				SetFileAttributesA(backup, FILE_ATTRIBUTE_HIDDEN);
				const char *cur = m_path.get(".old", temp);
				if (0 == _access(cur, 0)) // 删除已备份的旧文件
					DeleteFileA(cur);
				else MakeDir(cur);
				if (FALSE == CopyFileA(path, cur, FALSE))// 替换前先备份
				{
					char info[300];
					sprintf_s(info, "===> 接收文件前备份\"%s\"失败.\n", path);
					OutputDebugStringA(info);
				}
			}else if(bNotExist) MakeDir(path);
			FILE *fid = getFileId(path);
			if (NULL == fid) // 文件被占用
			{
				make_cmd(CMD, "down", "?");
				m_pSocket->sendData(CMD, SIZE_1);// 不下载
				char szLog[300];
				sprintf_s(szLog, "===> 本地文件被占用: %s\n", path);
				DeleteFileA(m_path.get(".old", temp));
				OutputDebugStringA(szLog);
				return false;
			}
			make_cmd(CMD, "down", buf);
			m_pSocket->sendData(CMD, SIZE_1); // 下载
			int total = size;
			const int SIZE_2 = 128*1024;// 128K
			char *buf = new char[SIZE_2];// recv buf
			int s = 0; // 超时次数
			do // 收文件
			{
				int recv = m_pSocket->recvData(buf, SIZE_2-1);
				if (recv > 0)
				{
					s = 0;
					buf[recv] = 0;
					total -= recv;
					fwrite(buf, recv, 1, fid);
				}
				else if (recv < 0)
				{
					OutputDebugStringA("===> Socket断开，接收文件失败.\n");
					break;
				}
				else if (10 == ++s)
				{
					OutputDebugStringA("===> Socket超时，接收文件失败.\n");
					break;
				}
			} while (total > 0);
			delete [] buf;
			fclose(fid);
			if (-1==isCommon && total)// 失败后还原备份文件
			{
				int k = 10;
				while (FALSE == DeleteFileA(path) && --k) Sleep(500);
				const char *cur = m_path.get(".old", temp);
				if (0 == _access(cur, 0) && FALSE == CopyFileA(cur, path, FALSE))
				{
					char info[300];
					sprintf_s(info, "===> 还原备份文件\"%s\"失败.\n", path);
					OutputDebugStringA(info);
				}
			}
			return 0 == total;
		}else{
			make_cmd(CMD, "down", "?");
			m_pSocket->sendData(CMD, SIZE_1);// 不下载
			char szLog[300];
			sprintf_s(szLog, "===> 本地文件不需要更新: %s\n", path);
			OutputDebugStringA(szLog);
			return TRUE==isCommon ? true : false;
		}
	}
	return false;
}
