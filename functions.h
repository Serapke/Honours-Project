#include <string>

void put(unsigned long long address, long long value);
long long get(unsigned long long address);
long long atomic_increment(std::string address, unsigned long long increment);
