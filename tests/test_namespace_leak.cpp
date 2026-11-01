// This file must NOT compile: without ARDUINO TinyNTP.h does not use the
// namespace, so the plain class names are not available. ctest builds it and
// expects an error.
#ifdef ARDUINO
#error "this test must be built without ARDUINO"
#endif
#include "mock.h"
#include "TinyNTP.h"

int main() {
  TinyNTPClient<MockUDP> client;  // error: not declared
  return 0;
}
