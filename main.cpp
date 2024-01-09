#include <iostream>

#include "transfer.hpp"

int main() {
	transfer.bindPort(1234, [&](std::string input) {
		std::cout << "Message: " << input;
		std::cout << "Response: ";
		std::string response;
		std::cin >> response;
		return response;
	});

	return 0;
}
