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

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

namespace file {
	struct packet {
		void* data;
		int size;
		bool dofree = false;

		packet(void* _data, int _size) : data(_data), size(_size) { }
		packet(void* _data, int _size, bool _free) : data(_data), size(_size), dofree(_free) { }
	};

	int type(char* _filename) {
		struct stat sb;
		if (stat(_filename, &sb) == -1) return -1;
		if (sb.st_mode & S_IFDIR) return 1;
		return 0;
	}

	bool exists(char* _filename) {
		return file::type(_filename) != -1;
	}

	file::packet read(char* _filename) {
		int typ = file::type(_filename);
		if (typ == 0) {
			std::ifstream sr;
			sr.open(_filename);
			sr.seekg(0, std::ios::end);
			file::packet _out = file::packet(malloc(sr.tellg()), sr.tellg(), true);
			sr.seekg(0);
			sr.read((char*)_out.data, _out.size);
			sr.close();
			return _out;
		}
		if (typ == 1) {
			std::string outS = "<html>\n<body>\n";

			std::vector<std::string> files = std::vector<std::string>();

			DIR* dirs = opendir(_filename);
			struct dirent* direntp;
			while ((direntp = readdir(dirs)) != NULL) {
				files.push_back(direntp->d_name);
			}
			closedir(dirs);

			std::sort(files.begin(), files.end());

			for (std::string file : files) {
				outS += "<a href=\"" + file + "\">" + file + "</a><br>\n";
			}

			outS += "</body>\n</html>";
			file::packet _out = file::packet(malloc(outS.length()), outS.length(), true);
			memcpy((char*)_out.data, outS.c_str(), _out.size);
			return _out;
		}
		return file::packet(_filename, strlen(_filename));
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

			if (response.dofree) free(response.data);
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

		void* _frP = nullptr;

		message() { }
		message(char* __msg, int _size) {
			char* _msg = (char*)malloc(_size + 1);
			_frP = (void*)_msg;
			memcpy(_msg, __msg, _size + 1);
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
					if (_msg[i] == '\r' || _msg[i] == '\n') {
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
		~message() { if (_frP != nullptr) free(_frP); }

		char* absolutePath() {
			bool _def = false;
			int strsize = headers - path + 1;
			char* _out = (char*)malloc(strsize);
			memcpy(_out, path, strsize);

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

		file::packet get() {
			int outCSsize = strlen(protocol) + strlen(path) + strlen(headers) + bodyLen + 5;
			char* outCS = (char*)malloc(outCSsize);
			memcpy(outCS, protocol, strlen(protocol) + 1);
			strcat(outCS, (char*)" ");
			strcat(outCS, path);
			strcat(outCS, (char*)" ");
			strcat(outCS, headers);
			strcat(outCS, (char*)"\n\n");
			strcat(outCS, body);
			file::packet _out(outCS, outCSsize, true);
			return _out;
		}
	};

	class server {
	public:
		char* HOME = (char*)".";

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
						path = (char*)malloc(strlen(absPath) + 3);
						path[0] = '.';
						memcpy(path + 1, absPath, strlen(absPath) + 1);
					}
					free(absPath);

					file::packet content = file::read(path);
					HTTP::message _out;
					_out.body = (char*)content.data;
					_out.bodyLen = content.size;
					_out.protocol = (char*)"HTTP/1.1";
					_out.path = (char*)"200 OK";
					_out.headers = (char*)"";
					return _out.get();
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
