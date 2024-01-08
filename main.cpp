#include <iostream>

#include "transfer.hpp"

int main() {
	transfer.bindPort(1234, [&](std::string input) {
		std::cout << input << std::endl;
		return "";
	});

	return 0;
}
