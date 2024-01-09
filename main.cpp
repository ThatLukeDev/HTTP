#include <iostream>

#include "transfer.hpp"

int main() {
	transfer.bindPort(1234, [&](transfer::packet input) {
		std::cout << "Message: " << (char*)input.data;
		std::cout << "Response: ";
		const char* response = "this is the full response";
		return transfer::packet((void*)response, strlen(response));
	});

	return 0;
}
