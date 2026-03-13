#ifndef DEBUGMSG_HPP
#define DEBUGMSG_HPP

#include <iostream>

#if defined(DEBUG_MESSAGES)

#define dbgmsg(msg) \
    do { \
        std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " << msg; \
    } while (0)

#else

#define dbgmsg(msg) do { } while (0)

#endif

#endif // DEBUGMSG_HPP