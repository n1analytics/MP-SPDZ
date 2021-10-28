#include <iostream>
#include <string>

std::string PPC_PREFIX;
int CONNETION_waiting_millisecond_FLAG = 0;
bool DEBUG_FLAG = false;
std::string PPC_DEBUG_PREFIEX = "PPC-LOG-";

std::string get_prefix() {
    return PPC_PREFIX;
}
void set_prefix(const std::string& str) {
    PPC_PREFIX = str;
}

int get_connection_waiting_millisecond_flag()
{
    return CONNETION_waiting_millisecond_FLAG;
}

void set_connection_waiting_millisecond_flag(int sleep_time)
{
    CONNETION_waiting_millisecond_FLAG = sleep_time;
}

bool get_debug_flag() {
    return DEBUG_FLAG;
}

void set_debug_flag(bool debug_flag) {
    DEBUG_FLAG = debug_flag;
}
