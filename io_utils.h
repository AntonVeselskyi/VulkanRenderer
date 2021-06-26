#pragma once

#include <iostream>

std::ostream& bold_on(std::ostream& os)
{
#if ENABLE_COLORING_IO
    return os << "\e[1m";
#else
    return os << "--> \t";
#endif
}

std::ostream& bold_off(std::ostream& os)
{
#if ENABLE_COLORING_IO
    return os << "\e[0m";
#else
    return os;
#endif
}