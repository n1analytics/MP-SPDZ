#include <iostream>
#include <string>

extern int SOCKET_FLAG;

extern std::string PPC_PREFIEX;

std::string get_prefix();
void set_prefix(const std::string& str);

int get_socket_flag();
void set_socket_flag(int sleep_time);