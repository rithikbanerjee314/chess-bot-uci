#include "position.hpp"
#include "magic.hpp"
#include "uci.hpp"

int main() {
  zobrist::init();
  magic::init();
  runUciLoop();
  return 0;
}
