// This file must NOT compile: with NO_TINY_NTP_NAMESPACE TinyNTP.h does not use
// the namespace, so the plain class names are not available. ctest builds it
// and expects an error.
#define NO_TINY_NTP_NAMESPACE
#include "mock.h"
#include "TinyNTP.h"

int main() {
  TinyNTPClient<MockUDP> client;  // error: not declared
  return 0;
}
