
/** Copyright notice \n
* Copyright (c) 2016, BCECR
* All rights reserved.
* @file		BcecrSocket.h
* @brief BCECR SOCKET
* @version	1.0
* @date unknow
* @update	2017/3/10
* @author	BCECR
*/

#pragma once
#include <WinSock2.h>
#include <string>

/// 最大监听客户端数
#define MAX_LISTEN 5
/// 默认缓冲区大小
#define DEFAULT_BUFFER 256
/************************************************************************/
/* socket 错误码定义                                                    */
/************************************************************************/
/// no error
#define _NO__ERROR 0
/// WSAStartup() error
#define ERROR_WSASTARTUP 1
/// socket() error
#define ERROR_SOCKET 2
/// connect() error
#define ERROR_CONNECT 3
/// bind() error
#define ERROR_BIND 4
/// listen() error
#define ERROR_LISTEN 5
/// ioctlsocket() error
#define ERROR_IOCTLSOCKET 6

/** 
* @class	CBcecrSocket 
* @brief	socket通信类，建立socket通信的服务器/客户端程序
* @details	实现基本的收/发数据的功能
*/
class CBcecrSocket
{
public:
	/// 构造函数
	CBcecrSocket();
	/// 析构
	~CBcecrSocket();
	/// 初始化socket
	int init(const char *pIp, int nPort, int nType); //0:server, 1:client
	int init(SOCKET s, const char *ip); // 从s创建客户端socket
	/// 只针对client端，连接服务端
	int Connect();
	/// socket退出时进行清理工作
	void unInit();

	/// 接收数据
	int recvData(char *pBuf, int nReadLen, int nTimeOut = 1000); //nTimeOut单位毫秒
	/// 发送数据
	int sendData(const char *pData, int nSendLen);
	/// 错误信息
	std::string GetErrorMsg(const int nRet) const;

	bool IsConnected() const { return m_bConneted; }

	bool IsRegistered() const { return m_bRegistered; } // 是否注册

	void Register(bool result) { m_bRegistered = result; } // 注册是否成功

	SOCKET getSocket() const { return m_Socket; }

private:
	bool m_bInit;			/**< 是否已经初始化 */
	bool m_bConneted;		/**< 是否已连接成功 */
	bool m_bRegistered;		/**< 是否注册成功 */
	int m_nType;			/**< socket类型，0:server, 1:client */

	SOCKET m_Socket;		/**< 作为客户端连接的socket */
	sockaddr_in m_in;

public:
	char m_chToIp[32];				/**< 对方的IP */
	int  m_chToport;				/**< 对方的端口 */
	char m_chLocalIp[32];			/**< 本地IP */
};
