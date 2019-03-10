#pragma once

#include <WinSock2.h>


class CSimpleSocket
{
public:

	inline CSimpleSocket() : m_Socket(INVALID_SOCKET), m_argp(1) { }

	inline ~CSimpleSocket() { Close(); }

	// 连接服务端（非阻塞模式）
	int connectServer(const char *pIp, int nPort);

	inline SOCKET getSocket() { SOCKET s = m_Socket; m_Socket = INVALID_SOCKET; return s; }

	inline SOCKET GetSocket() const { return m_Socket; }

	void Close(); // 关闭

	int sendData(const char *pData, int nSendLen);

	int recvData(char *pBuf, int nReadLen);

private:
	SOCKET m_Socket;/**< 作为客户端连接的socket */
	unsigned long m_argp;/**< 是否阻塞模式 1-阻塞 0-非阻塞*/
};
