#pragma once
#include "BcecrSocket.h"

// 目录
#define IS_DIR FILE_ATTRIBUTE_DIRECTORY

// 目录访问器[成员易变的]
typedef struct folder
{
	char buf[_MAX_PATH], *pos;
	folder()
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
}folder;

/************************************************************************
* @class Downloader
* @brief 基于SOCKET的文件下载器
* @details 下载流程如下（2个来回）
* 1、与服务器建立连接
* 2、发送要下载的文件，格式：size:file
* 3、收取文件头数据，格式：size:bytes,md5
* 4、发送下载指令，格式：down:file
* 5、收取文件
************************************************************************/
class Downloader
{
private:
	bool m_bConnect;				// 是否连接服务器
	bool m_bCleanup;				// .old是否已清理
	CBcecrSocket *m_pSocket;		// 通讯套接字
	folder m_path;					// 目录访问器

	char m_strApp[64];				// 被升级程序名称

	bool check(const char *file, int size, const char *MD5); // 是否已下载

public:
	Downloader(void);
	~Downloader(void);
	void Connect(const char *ip, int port);
	void Disconnect();
	void SetUpdateApp(const char *name); // 设置升级程序

	/**
	* @brief 下载文件
	* @param[in] name 文件名称
	* @param[in] postfix 缓存文件后缀
	* @param[in] isCommon 是否公用文件（守护：取1 filelist：取-1，非守护：取0）
	* @param[in] type 文件类型
	*/
	bool DownloadFile(const char *name, const char *postfix = ".exe", BOOL isCommon = false, const char *type = "exe");
};
