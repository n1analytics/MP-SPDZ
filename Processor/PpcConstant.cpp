#include <iostream>
#include <string>

std::string PPC_PREFIX;
int SOCKET_FLAG = 0;

std::string get_prefix() {
    return PPC_PREFIX;
}
void set_prefix(const std::string& str) {
    PPC_PREFIX = str;
}

int get_socket_flag()
{
    return SOCKET_FLAG;
}

void set_socket_flag(int sleep_time)
{
    SOCKET_FLAG = sleep_time;
}