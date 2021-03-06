// ServerApplication.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "pch.h"
#include <iostream>

#include <WinSock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>
#include <thread>

#include <mswsock.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"


typedef struct
{
	WSAOVERLAPPED Overlapped;
	SOCKET Socket;
	WSABUF wsaBuf;
	char Buffer[1024];
	DWORD BytesSent;
	DWORD BytesToSend;
} PER_IO_DATA, *LPPER_IO_DATA;


static DWORD WINAPI ServerWorkerThread(LPVOID lpParameter)
{
	HANDLE hCompletionPort = (HANDLE)lpParameter;
	DWORD NumBytesSent = 0;
	ULONG CompletionKey;
	LPPER_IO_DATA PerIoData;

	while (GetQueuedCompletionStatus(hCompletionPort, &NumBytesSent, &CompletionKey, (LPOVERLAPPED*)&PerIoData, INFINITE))
	{
		if (!PerIoData)
			continue;

		if (NumBytesSent == 0)
		{
			std::cout << "Client disconnected!\r\n\r\n";
		}
		else
		{
			PerIoData->BytesSent += NumBytesSent;
			if (PerIoData->BytesSent < PerIoData->BytesToSend)
			{
				PerIoData->wsaBuf.buf = &(PerIoData->Buffer[PerIoData->BytesSent]);
				PerIoData->wsaBuf.len = (PerIoData->BytesToSend - PerIoData->BytesSent);
			}
			else
			{
				PerIoData->wsaBuf.buf = PerIoData->Buffer;
				PerIoData->wsaBuf.len = strlen(PerIoData->Buffer);
				PerIoData->BytesSent = 0;
				PerIoData->BytesToSend = PerIoData->wsaBuf.len;
			}

			if (WSASend(PerIoData->Socket, &(PerIoData->wsaBuf), 1, &NumBytesSent, 0, &(PerIoData->Overlapped), NULL) == 0)
				continue;

			if (WSAGetLastError() == WSA_IO_PENDING)
				continue;
		}

		closesocket(PerIoData->Socket);
		delete PerIoData;
	}

	return 0;
}

int SampleMain() {
	WSADATA WsaDat;
	if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0)
		return 0;

	HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!hCompletionPort)
		return 0;

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	for (DWORD i = 0; i < systemInfo.dwNumberOfProcessors; ++i)
	{
		HANDLE hThread = CreateThread(NULL, 0, ServerWorkerThread, hCompletionPort, 0, NULL);
		CloseHandle(hThread);
	}

	SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET)
		return 0;

	SOCKADDR_IN server;
	ZeroMemory(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(27015);

	if (bind(listenSocket, (SOCKADDR*)(&server), sizeof(server)) != 0)
		return 0;

	if (listen(listenSocket, 1) != 0)
		return 0;

	std::cout << "Waiting for incoming connection...\r\n";

	SOCKET acceptSocket;
	do
	{
		sockaddr_in saClient;
		int nClientSize = sizeof(saClient);
		acceptSocket = WSAAccept(listenSocket, (SOCKADDR*)&saClient, &nClientSize, NULL, NULL);
	} while (acceptSocket == INVALID_SOCKET);

	std::cout << "Client connected!\r\n\r\n";

	CreateIoCompletionPort((HANDLE)acceptSocket, hCompletionPort, 0, 0);

	LPPER_IO_DATA pPerIoData = new PER_IO_DATA;
	ZeroMemory(pPerIoData, sizeof(PER_IO_DATA));

	strcpy_s(pPerIoData->Buffer, "Welcome to the server!\r\n");

	pPerIoData->Overlapped.hEvent = WSACreateEvent();
	pPerIoData->Socket = acceptSocket;
	pPerIoData->wsaBuf.buf = pPerIoData->Buffer;
	pPerIoData->wsaBuf.len = strlen(pPerIoData->Buffer);
	pPerIoData->BytesToSend = pPerIoData->wsaBuf.len;

	DWORD dwNumSent;
	if (WSASend(acceptSocket, &(pPerIoData->wsaBuf), 1, &dwNumSent, 0, &(pPerIoData->Overlapped), NULL) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			delete pPerIoData;
			return 0;
		}
	}

	while (TRUE)
		Sleep(1000);

	shutdown(acceptSocket, SD_BOTH);
	closesocket(acceptSocket);

	WSACleanup();
	return 0;
}
int main()
{

    std::cout << "Hello World!\n"; 

	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;


	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	LPPER_IO_DATA PerIoData;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	auto completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	// Create a SOCKET for connecting to server
	ListenSocket = WSASocket(result->ai_family, result->ai_socktype, result->ai_protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	completionPort = CreateIoCompletionPort(
		(HANDLE)ListenSocket,
		completionPort,
		0,
		0);

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	std::thread acceptThread([&ListenSocket, &completionPort]() {

		while (true) {
			SOCKET AcceptSocket = INVALID_SOCKET;
			sockaddr_in saClient;
			int nClientSize = sizeof(saClient);

			if ((AcceptSocket = WSAAccept(ListenSocket, (SOCKADDR*)&saClient, &nClientSize, NULL, 0)) == SOCKET_ERROR)
			{
				printf("WSAAccept() failed with error %d\n", WSAGetLastError());
				return 1;
			}
			else
				printf("WSAAccept() looks fine!\n");

			CreateIoCompletionPort(
				(HANDLE)AcceptSocket,
				completionPort,
				0,
				0);

			LPPER_IO_DATA pPerIoData = new PER_IO_DATA;
			DWORD recvBytes = 0;
			DWORD flags = 0;

			ZeroMemory(pPerIoData, sizeof(PER_IO_DATA));


			ZeroMemory(&(pPerIoData->Overlapped), sizeof(OVERLAPPED));
			pPerIoData->BytesSent = 0;
			pPerIoData->BytesToSend = 0;
			pPerIoData->wsaBuf.len = 1024;
			pPerIoData->wsaBuf.buf = pPerIoData->Buffer;
			pPerIoData->Socket = AcceptSocket;


			// This is an async call
			auto recResult = WSARecv(
				AcceptSocket,
				&(pPerIoData->wsaBuf),
				1,
				&recvBytes,
				&flags,
				&(pPerIoData->Overlapped),
				NULL);
		}
	});

	std::thread t2([&]() {
		while (true)
		{
			//SOCKADDR_IN saRemote;
			//int RemoteLen = sizeof(saRemote);;

			DWORD numberOfBytes = 0;
			unsigned long completionKey = 0;
			OVERLAPPED overLapped;
			LPOVERLAPPED overLappedptr = &overLapped;
			LPPER_IO_DATA PerIoData;

			if (completionPort == nullptr) continue;

			auto info = GetQueuedCompletionStatus(
				completionPort,
				&numberOfBytes,
				&completionKey,
				(LPOVERLAPPED*)&PerIoData,
				10000 //  dwMilliseconds
			);
			DWORD dwError = GetLastError();
			//Sleep(1000);
			if (numberOfBytes > 0) {
				std::cout << "overlapped is not null" << std::endl;
			}
			if (PerIoData != NULL) {
				std::cout << "overlapped is not null" << std::endl;
				closesocket(PerIoData->Socket);
				delete PerIoData;

				//break;
			}
		}


	});



	while (true) {
		Sleep(1000);
	}

	//t.join();
	CloseHandle(completionPort);

	closesocket(ListenSocket);
	//closesocket(AcceptSocket);
	WSACleanup();

	return 0;
	//// Accept a client socket
	//AcceptSocket = accept(ListenSocket, NULL, NULL);
	//if (AcceptSocket == INVALID_SOCKET) {
	//	printf("accept failed with error: %d\n", WSAGetLastError());
	//	closesocket(ListenSocket);
	//	WSACleanup();
	//	return 1;
	//}

	//// No longer need server socket
	//closesocket(ListenSocket);

	//// Receive until the peer shuts down the connection
	//do {

	//	iResult = recv(AcceptSocket, recvbuf, recvbuflen, 0);
	//	if (iResult > 0) {
	//		printf("Bytes received: %d\n", iResult);

	//		// Echo the buffer back to the sender
	//		iSendResult = send(AcceptSocket, recvbuf, iResult, 0);
	//		if (iSendResult == SOCKET_ERROR) {
	//			printf("send failed with error: %d\n", WSAGetLastError());
	//			closesocket(AcceptSocket);
	//			WSACleanup();
	//			return 1;
	//		}
	//		printf("Bytes sent: %d\n", iSendResult);
	//	}
	//	else if (iResult == 0)
	//		printf("Connection closing...\n");
	//	else {
	//		printf("recv failed with error: %d\n", WSAGetLastError());
	//		closesocket(AcceptSocket);
	//		WSACleanup();
	//		return 1;
	//	}

	//} while (iResult > 0);

	//// shutdown the connection since we're done
	//iResult = shutdown(AcceptSocket, SD_SEND);
	//if (iResult == SOCKET_ERROR) {
	//	printf("shutdown failed with error: %d\n", WSAGetLastError());
	//	closesocket(AcceptSocket);
	//	WSACleanup();
	//	return 1;
	//}

	//// cleanup
	//closesocket(AcceptSocket);
	//WSACleanup();

	//return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
