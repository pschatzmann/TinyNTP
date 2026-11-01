// main() is in its own file: the Arduino API (which test_device.cpp uses)
// declares main() without arguments, so a file that includes it cannot define
// main() with the command line arguments.
int device_main(int argc, char** argv);

int main(int argc, char** argv) { return device_main(argc, argv); }
