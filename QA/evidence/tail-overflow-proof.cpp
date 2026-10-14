#include <cmath>
#include <cstdint>
#include <limits>
int main(){volatile double sr=std::numeric_limits<double>::max();volatile std::uint32_t tail=static_cast<std::uint32_t>(std::ceil(2.0*sr));return tail==0;}
