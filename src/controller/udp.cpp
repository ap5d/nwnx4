/***************************************************************************
    udp.cpp - A small class for udp socket communication
    Copyright (C) 2003 Ingmar Stieger (Papillon)
    email: papillon@nwnx.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ***************************************************************************/

#include "udp.h"
#include <cstring>

CUDP::CUDP(const char *szAddress, int port)
{
	// Load Winsock
	WSAStartup(MAKEWORD(1,1), &wsda);

	// Create a UDP socket
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// Error?
	if(s == SOCKET_ERROR)
	{
		MessageBox(
                nullptr,
                L"Error while creating UDP socket.", L"NWNX2 Watchdog", NULL);
		return;
	}

	// Fill in the host information
	addr.sin_family = AF_INET;
	setPort(port);
	setAddress(szAddress);

	// Set socket to non-blocking
	unsigned long enable = 1;
	ioctlsocket(s, FIONBIO, &enable);
}

CUDP::~CUDP()
{
    char buf;
    int err = recv(s, &buf, 1, MSG_PEEK);
    if (err != SOCKET_ERROR)
    {
        closesocket(s);
        WSACleanup();
    }
}

void CUDP::setPort(int port)
{
	addr.sin_port = htons(port);
}

void CUDP::setAddress(const char *szAddress)
{
	if(inet_pton(AF_INET, szAddress, (void*) addr.sin_addr.s_addr) != 1)
	{
		// The address wasn't in numeric form, resolve it
		host = nullptr;
		host = gethostbyname(szAddress);	// Get the IP address of the server and store it in host
		if(host == nullptr)
		{
			MessageBox(nullptr, L"Unknown host.", L"NWNX2 Watchdog", NULL);
			return;
		}
		memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
	}
}

void CUDP::sendMessage(const char* message)
{
	sendto(s, message, (int)strlen(message), 0, (struct sockaddr *) &addr, sizeof(addr));
}

int CUDP::getMessage(char* message, int len)
{
	int ret;
	ret = recvfrom(s, message, len, 0, nullptr, nullptr);

	if (ret == SOCKET_ERROR)
		return 0;
	else
		return ret;
}