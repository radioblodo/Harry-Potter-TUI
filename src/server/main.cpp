#include "common/types.hpp"

#include <iostream>

int main() {
  std::cout << "hp-server " << HOGWARTS_VERSION << "  ("
            << hogwarts::kProtocolName << " v" << hogwarts::kProtocolVersion
            << ")\n";
  std::cout << "Stub: Unix-socket rooms come in a later phase.\n";
  std::cout << "Until then, play offline with ./hp-session\n";
  return 0;
}
