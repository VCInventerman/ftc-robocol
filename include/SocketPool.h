#if !defined(LIBROBOCOL_SOCKETPOOL_H)
#define LIBROBOCOL_SOCKETPOOL_H

#include <vector>
#include <algorithm>
#include <variant>
#include <cassert>

#include <network.h>

#include "sync.h"
#include "Socket.h"

namespace librobocol
{
    // Class that pools libogc net sockets to poll them all at once
    // Cannot be applicable to all files since the net_ function prefix does not apply to the filesystem
    class SocketPool
    {
    public:
        static Mutex accessM;
        static std::vector<std::reference_wrapper<LibogcNetSocket>> sockets;
        static std::vector<pollsd> polls;

        // libogc net_ sockets
        template <typename SockT>
        typename std::enable_if_t<std::is_base_of_v<LibogcNetSocket, SockT>>
        static add(SockT &sock)
        {
            auto l = LockGuardMutex(accessM);

            if (std::find(sockets.begin(), sockets.end(), (LibogcNetSocket&)sock) == sockets.end())
            {
                sockets.push_back(sock);
                polls.push_back({.socket = sock.getSocketHandle(), .events = POLLIN | POLLOUT, .revents = 0});
            }
        }

        static void tick()
        {
            auto l = LockGuardMutex(accessM);

            assert(sockets.size() == polls.size());

            if (polls.size() > 0)
            {
                int res = net_poll(polls.data(), polls.size(), 0);

                if (res < 0)
                {
                    perror("Sock poll error");
                }

                for (size_t i = 0; i < polls.size(); i++)
                {
                    pollsd &poll = polls[i];
                    LibogcNetSocket &sock = sockets[i];

                    sock.handlePollResult(poll.revents);
                }

                reset();
            }

            
        }

        static void reset()
        {
            for (auto &i : polls)
            {
                i.revents = POLLNVAL;
            }
        }
    };
    Mutex SocketPool::accessM = {};
    std::vector<std::reference_wrapper<LibogcNetSocket>> SocketPool::sockets = {};
    std::vector<pollsd> SocketPool::polls;
}

#endif // if !defined(LIBROBOCOL_SOCKETPOOL_H)