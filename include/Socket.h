#if !defined(LIBROBOCOL_SOCKET_H)
#define LIBROBOCOL_SOCKET_H

#include "FixedBuf.h"

namespace librobocol
{
    class Socket
    {
        public:

        enum class InterfaceType
        {
            NONE,
            POSIX,
            WINSOCK,
            LIBOGC_NET
        };

        virtual InterfaceType getInterfaceType() const noexcept = 0;

        // Put a packet on the socket to be output later
        virtual void write(FixedBuf &&buf) = 0;
    };

    class LibogcNetSocket : public Socket
    {
        public:

        InterfaceType getInterfaceType() const noexcept { return InterfaceType::LIBOGC_NET; } 

        // Get the underlying socket handle
        virtual int getSocketHandle() const noexcept = 0;

        // Tell the socket what new operations are available
        virtual void handlePollResult(int res) = 0;

        std::strong_ordering operator<=> (const LibogcNetSocket& lhs) const
        {
            return getSocketHandle() <=> lhs.getSocketHandle();
        }

        bool operator== (const LibogcNetSocket& lhs) const 
        {
            return getSocketHandle() == lhs.getSocketHandle();
        }
    };
}

#endif // if !defined(LIBROBOCOL_SOCKET_H)