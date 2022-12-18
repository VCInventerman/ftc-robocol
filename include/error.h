#if !defined(LIBROBOCOL_ERROR_H)
#define LIBROBOCOL_ERROR_H

#ifdef GEKKO
#include <cstdio>
#else
#include <iostream>
#endif


namespace librobocol
{
    template <typename MsgT>
    void reportError(MsgT& msg)
    {
        #ifdef GEKKO
        printf("%s\n", msg);
        #else
        std::coud << msg << '\n';
        #endif
    }
}

#endif // if !defined(LIBROBOCOL_ERROR_H)