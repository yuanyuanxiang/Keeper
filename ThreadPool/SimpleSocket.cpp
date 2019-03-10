#include "stdafx.h"
#include "SimpleSocket.h"   
#include <stdio.h>
#include <iostream>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#define OUTPUT TRACE
#else
#define OUTPUT
#endif


void CSimpleSocket::Close()
{
	if (INVALID_SOCKET != m_Socket)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
	}
}


// 非阻塞式connect的Windows实现
// https://blog.csdn.net/u014585564/article/details/53287026
int CSimpleSocket::connectServer(const char *pIp, int nPort)
{
	if (INVALID_SOCKET == m_Socket)
	{
		m_Socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (INVALID_SOCKET == m_Socket)
			return -1;
		const int nSendBuf = 1024 * 1024 * 2;
		::setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, (const char*)&nSendBuf, sizeof(int));
		const int nRecvBuf = 1024 * 1024 * 2;
		::setsockopt(m_Socket, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));
		ioctlsocket(m_Socket, FIONBIO, &m_argp);//将socket设置为阻塞模式
	}
	if (0 == m_argp)
	{
		m_argp = 1;
		ioctlsocket(m_Socket, FIONBIO, &m_argp);
	}

	sockaddr_in m_in = { 0 };
	m_in.sin_family = AF_INET;
	m_in.sin_port = htons(nPort);
	m_in.sin_addr.s_addr = inet_addr(pIp);

	if ( (connect(m_Socket, (const SOCKADDR *)&m_in, sizeof(SOCKADDR)) == SOCKET_ERROR ) 
		&& (WSAGetLastError() == WSAEWOULDBLOCK)) 
	{
		const struct timeval tv = { 0, 100 * 1000 };
		fd_set writefds,expectfds;
		FD_ZERO(&writefds);
		FD_ZERO(&expectfds);
		FD_SET(m_Socket,&writefds);
		FD_SET(m_Socket,&expectfds);

		int result = select(m_Socket + 1, NULL, &writefds, &expectfds, &tv);
		if (result > 0) 
		{
			if(FD_ISSET(m_Socket,&writefds))
				OUTPUT("connect success!\n");
			if(FD_ISSET(m_Socket,&expectfds))
			{
				OUTPUT("connect failed!\n");
				int error, error_len;
				error_len = sizeof(error);
				getsockopt(m_Socket, SOL_SOCKET, SO_ERROR, (char *)&error, &error_len);//获得错误号
				OUTPUT("error is: %d\n",error);
				return -1;
			}
		}
		else if(result == 0)
		{
			OUTPUT("connect timeout\n");
			return -1;
		}
		else
		{
			OUTPUT("select() error: %d\n",WSAGetLastError());
			return -1;  
		}
	}
	else
	{
		OUTPUT("connect() error: %d\n",WSAGetLastError());
		return -1;  		
	}

	m_argp = 0;
	ioctlsocket(m_Socket, FIONBIO, &m_argp);//将socket恢复为非阻塞模式

	return 0;
}


int CSimpleSocket::sendData(const char *pData, int nSendLen)
{
	int nRet = 0;

	const struct timeval time = { 1, 0 };
	fd_set fdSend;
	int nLen = nSendLen;
	const char *pTmp = pData;

	while (nLen > 0)
	{
		FD_ZERO(&fdSend);
		FD_SET(m_Socket, &fdSend);

		int ret = ::select(m_Socket+1, NULL, &fdSend, NULL, &time);
		if ( 1== ret )
		{
			if ( FD_ISSET(m_Socket, &fdSend) )
			{
				ret = ::send(m_Socket, pTmp, nLen, 0);
				if (ret <= 0)
				{
					nRet = -1;
					break;
				}

				nLen -= ret;
				pTmp += ret;

			}
		}
		else if ( ret < 0)
		{
			nRet = ret;
			break;
		}
		else if (0 == ret)
		{
			nRet = 0;
			break;
		}
	}

	return nRet;
}


int CSimpleSocket::recvData(char *pBuf, int nReadLen)
{
	const struct timeval time = { 3, 0 };

	fd_set fd;
	FD_ZERO(&fd);
	FD_SET(m_Socket, &fd);

	int ret = ::select(m_Socket+1, &fd, NULL, NULL, &time);
	if ( ret )
	{
		if ( FD_ISSET(m_Socket, &fd) )
		{
			ret = ::recv(m_Socket, pBuf, nReadLen, 0);
			ret = (ret <= 0) ? -1 : ret;
		}
	}
	else if(ret < 0)
	{
		ret = -1;
	}

	return ret;
}
