#ifndef TRANSFER_HPP
#define TRANSFER_HPP

#include <cstring>
#include <string>
#include <vector>
#include <functional>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static class transfer {

	const int BUFFER_SIZE = 1024;
	const int MAX_AWAITS = 5;

	std::vector<int> sockets = std::vector<int>();
	std::vector<int> ports = std::vector<int>();

public:

	struct packet {
		void* data;
		int size;

		packet(void* _data, int _size) : data(_data), size(_size) { }
	};

	int openSocket(unsigned int _port) {
		int newSocket = socket(AF_INET, SOCK_STREAM, 0);

		if (newSocket < 0) {
			std::clog << "Error: Failed to open socket\n";
			return -1;
		}

		sockets.push_back(newSocket);
		ports.push_back(_port);

		sockaddr_in address = sockaddr_in();
		address.sin_family = AF_INET;
		address.sin_port = htons(_port);
		address.sin_addr.s_addr = INADDR_ANY;

		if (bind(newSocket, (sockaddr*)&address, sizeof(address)) < 0) {
			std::clog << "Error: Failed to bind socket\n";
			return -1;
		}

		return newSocket;
	}

	void closeSocket(unsigned int _socket) {
		close(_socket);
		for (int i = 0; i < sockets.size(); i++) {
			if (sockets[i] == _socket) {
				sockets.erase(std::next(sockets.begin(), i));
				ports.erase(std::next(ports.begin(), i));
			}
		}
	}

	int bindSocket(unsigned int _socket, std::function<packet(packet)> func) {
		while (std::find(sockets.begin(), sockets.end(), _socket) != sockets.end()) {
			listen(_socket, MAX_AWAITS);
			sockaddr_in address = sockaddr_in();
			socklen_t addresslength = sizeof(address);

			int connection = accept(_socket, (sockaddr*)&address, &addresslength);
			if (connection < 0) {
				std::clog << "Error: Failed to accept connection\n";
				return -1;
			}

			char buffer[BUFFER_SIZE];
			for (int i = 0; i < BUFFER_SIZE; i++) {
				buffer[i] = (char)0;
			}

			if (read(connection, buffer, BUFFER_SIZE - 1) < 0) {
				std::clog << "Error: Failed to recieve data from connection\n";
				return -1;
			}

			packet response = func(packet(&buffer, strlen((char*)&buffer)));

			if (response.size == 0) {
				continue;
			}
			if (response.size == -1) {
				break;
			}

			if (write(connection, response.data, response.size) < 0) {
				std::clog << "Error: Failed to send data to connection\n";
				return -1;
			}

			close(connection);
		}
		closeSocket(_socket);

		return 0;
	}

	void closePort(unsigned int _port) {
		for (int i = 0; i < ports.size(); i++) {
			if (ports[i] == _port) {
				closeSocket(sockets[i]);
			}
		}
	}

	int bindPort(unsigned int _port, std::function<packet(packet)> func) {
		int _socket = -1;

		for (int i = 0; i < ports.size(); i++) {
			if (ports[i] == _port) {
				_socket = sockets[i];
			}
		}

		if (_socket < 0) {
			_socket = openSocket(_port);
			bindSocket(_socket, func);
			return _socket;
		}

		return bindSocket(_socket, func);
	}

} transfer;

#endif
