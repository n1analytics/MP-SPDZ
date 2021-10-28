#include <iostream>
#include <string>

extern int CONNETION_waiting_millisecond_FLAG;

extern bool DEBUG_FLAG;

extern std::string PPC_PREFIEX;

extern std::string PPC_DEBUG_PREFIEX;

std::string get_prefix();
void set_prefix(const std::string& str);

int get_connection_waiting_millisecond_flag();
void set_connection_waiting_millisecond_flag(int sleep_time);

bool get_debug_flag();
void set_debug_flag(bool debug_flag);
