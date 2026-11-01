// main() is in its own file: the Arduino API (which tinyntp.cpp uses) declares
// main() without arguments, so a file that includes it cannot define main()
// with the command line arguments.
int tinyntp_main(int argc, char** argv);

int main(int argc, char** argv) { return tinyntp_main(argc, argv); }
