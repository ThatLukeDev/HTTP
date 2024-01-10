#ifndef TRANSFER_HPP
#define TRANSFER_HPP

#include <string>
#include <cstring>
#include <vector>
#include <functional>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <iostream>
#include <fstream>

namespace file {
	struct packet {
		void* data;
		int size;

		packet(void* _data, int _size) : data(_data), size(_size) { }
	};

	file::packet read(char* _filename) {
		std::ifstream sr;
		sr.open(_filename);
		sr.seekg(0, std::ios::end);
		file::packet _out = file::packet(malloc(sr.tellg()), sr.tellg());
		sr.seekg(0);
		sr.read((char*)_out.data, _out.size);
		sr.close();
		return _out;
	}
}

static class transfer {

	const int BUFFER_SIZE = 1024;
	const int MAX_AWAITS = 5;

	std::vector<int> sockets = std::vector<int>();
	std::vector<int> ports = std::vector<int>();

public:

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

	int bindSocket(unsigned int _socket, std::function<file::packet(file::packet)> func) {
		while (1|| std::find(sockets.begin(), sockets.end(), _socket) != sockets.end()) {
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

			int strsize = BUFFER_SIZE - 1;
			for (int i = 0; i < BUFFER_SIZE - 1; i++) {
				if (buffer[i] == 0) {
					strsize = i;
				}
			}
			file::packet response = func(file::packet(&buffer, strsize));

			if (response.size == -256) {
				break;
			}
			if (response.size <= 0) {
				continue;
			}

			if (write(connection, response.data, response.size) < 0) {
				std::clog << "Error: Failed to send data to connection\n";
				return -1;
			}

			close(connection);
		}

		return 0;
	}

	void closePort(unsigned int _port) {
		for (int i = 0; i < ports.size(); i++) {
			if (ports[i] == _port) {
				closeSocket(sockets[i]);
			}
		}
	}

	int bindPort(unsigned int _port, std::function<file::packet(file::packet)> func) {
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

namespace HTTP {

	class message {
	public:
		char* protocol;
		char* path;
		char* headers;
		char* body;
		int bodyLen;

		message() { }
		message(char* __msg, int _size) {
			char* _msg = (char*)malloc(strlen(__msg));
			memcpy(_msg, __msg, strlen(__msg));
			char* msgS[4];
			int v = 0;
			int b = -1;
			msgS[0] = _msg;
			for (int i = 0; i < _size; i++) {
				if (v < 2) {
					if (_msg[i] == ' ') {
						v++;
						msgS[v] = (char*)((unsigned long)_msg + (unsigned long)(i + 1));
						*(msgS[v] - 1) = 0;
					}
				}
				else {
					if (_msg[i] == '\n') {
						b++;
						if (b == 0) {
							v++;
							if (v > 3) continue;
							msgS[v] = _msg + ((i + 1) * sizeof(char));
							*(msgS[v] - 1) = 0;
						}
						if (b == 2) {
							v++;
							if (v > 3) continue;
							msgS[v] = _msg + ((i + 1) * sizeof(char));
							*(msgS[v] - 2) = 0;
						}
					}
					else {
						b = 0;
					}
				}
			}
			protocol = msgS[0];
			path = msgS[1];
			headers = msgS[2];
			body = msgS[3];
			bodyLen = _size - (int)(body - protocol);
		}

		char* absolutePath() {
			bool _def = false;
			char* _out = (char*)malloc(strlen(path));
			memcpy(_out, path, strlen(path));

			if (_out[0] != '/') {
				_def = true;
			}
			for (int i = 0; i < strlen(_out); i++) {
				if (_out[i] == '.' && _out[i + 1] == '.' && _out[i + 2] == '/') {
					_def = true;
				}
			}

			if (_def) {
				_out[0] = '/';
				_out[1] = 0;
				return _out;
			}

			return _out;
		}
	};

	class server {
	public:
		char* HOME = (char*)"./index.html";

		int port = 80;
		bool running = false;

		server() { }
		server(int _port) : port(_port) { }
		~server() { if (running) stop(); }

		void start() {
			running = true;
			transfer.bindPort(port, [&](file::packet _data) {
				HTTP::message msg = HTTP::message((char*)_data.data, _data.size);

				if (strcmp(msg.protocol, "GET") == 0) {
					char* absPath = msg.absolutePath();
					char* path;
					if (strcmp(absPath, "/") == 0) {
						path = HOME;
					}
					else {
						path = (char*)malloc(strlen(absPath) + 1);
						path[0] = '.';
						memcpy(path + 1, absPath, strlen(absPath));
					}

					return file::read(path);
				}

				return _data;
			});
		}
		void stop() {
			transfer.closePort(port);
		}
	};
}

#endif
