#include <iostream>
#include <string>

std::string PPC_PREFIX;


std::string get_prefix() {
    return PPC_PREFIX;
}
void set_prefix(const std::string& str) {
    PPC_PREFIX = str;
}