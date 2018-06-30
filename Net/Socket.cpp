#include "Socket.h"
#include <stdexcept>

using namespace Net;

Socket::Socket(Socket && socket)
{
	handle = socket.handle;
	socketaddress = socket.socketaddress;
	socket.handle = -1;
}

Socket::Socket(SOCKET handle, const std::shared_ptr<sockaddr> & socketaddress)
{
	this->handle = handle;
	this->socketaddress = socketaddress;
}

Socket::~Socket() {
	if (handle != -1) {
		shutdown(handle, 2);
		closesocket(handle);
		handle = -1;
	}
}

std::unique_lock<std::mutex> Socket::GetWriteLock() {
	return std::unique_lock<std::mutex>(writelock);
}

std::unique_lock<std::mutex> Socket::GetReadLock() {
	return std::unique_lock<std::mutex>(readlock);
}

SOCKET Socket::GetHandle() {
	return handle;
}

std::string Socket::GetAddress() {
	char buf[255];
	return std::string(inet_ntop(socketaddress->sa_family, socketaddress.get(), buf, sizeof(buf)));
}

uint16_t Socket::GetPort() {
	if (socketaddress->sa_family == AF_INET) {
		return ntohs((*(sockaddr_in*)socketaddress.get()).sin_port);
	}
	else if (socketaddress->sa_family == AF_INET6) {
		return ntohs((*(sockaddr_in6*)socketaddress.get()).sin6_port);
	}
	return -1;
}

void Socket::SetProtocol(const std::string & protocol) {
	this->protocol = protocol;
}

std::string Socket::GetProtocol() {
	return protocol;
}

int Socket::Receive(uint8_t * buffer, int length) {
	return recv(handle, (char*)buffer, length, 0);
}

bool Socket::ReceiveAll(uint8_t * buffer, int length) {
	uint8_t * end = buffer + length;
	while (buffer < end) {
		int received = Receive(buffer, end - buffer);
		if (received <= 0) {
			return false;
		}
		buffer += received;
	}
	return true;
}

int Socket::Send(const uint8_t * buffer, int length) {
	return send(handle, (const char*)buffer, length, 0);
}

int Socket::Send(std::vector<uint8_t> buffer) {
	return Send(buffer.data(), buffer.size());
}

int Net::Socket::Send(std::vector<uint8_t> buffer, int length) {
	return Send(buffer.data(), length);
}

bool Socket::SendAll(const uint8_t * buffer, int length) {
	return Send(buffer, length) == length;
}

bool Socket::SendAll(std::vector<uint8_t> buffer) {
	return SendAll(buffer.data(), buffer.size());
}

bool Socket::SendAll(std::vector<uint8_t> buffer, int length) {
	return SendAll(buffer.data(), length);
}

int Socket::SendTo(uint8_t * buffer, int length, const sockaddr * to, socklen_t tolength) {
	return sendto(handle, (char*)buffer, length, 0, to, tolength);
}

int Socket::ReceiveFrom(uint8_t * buffer, int length, sockaddr * from, socklen_t * fromlength) {
	return recvfrom(handle, (char*)buffer, length, 0, from, fromlength);
}