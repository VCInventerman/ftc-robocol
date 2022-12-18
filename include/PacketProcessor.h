#ifndef LIBROBOCOL_PACKETPROCESSOR_H
#define LIBROBOCOL_PACKETPROCESSOR_H

#include <memory>
#include <array>
#include <queue>
#include <functional>
#include <list>

#include "sync.h"
#include "FixedBuf.h"
#include "BufCache.h"


// Do something with a packet of a type and provide it an environment of a type given by the template
// This means the handlers can be global
template <typename EnvT>
class PacketHandler
{
public:
    virtual size_t process(EnvT &conn, const char *begin, const char *end) = 0;

    virtual ~PacketHandler() = default;
};

// Sends completed packets to their handlers
template <typename EnvT>
class PacketProcessor
{
    // Functors capable of handling each type of packet
    std::vector<std::unique_ptr<PacketHandler<EnvT>>> handlers;

public:
    void addHandler(std::unique_ptr<PacketHandler<EnvT>> &&handler, EnvT::MsgType type)
    {
        /*if (type >= MsgType::COUNT || (int)type < 0)
        {
            throw std::out_of_range("Packet message type out of range")
        }*/

        if (handlers.size() <= (size_t)type)
        {
            handlers.resize((size_t)type + 1);
        }

        handlers.at((size_t)type) = std::move(handler);
    }

    // Process a buffer of packets, which may contain more than one.
    void process(EnvT &env, const char *begin, const char *end)
    {
        assert(end > begin);

        typename EnvT::MsgType type = env.peekType(begin, end);

        // printf("Message of type %d\n", type);

        if ((size_t)type < handlers.size() && handlers[(size_t)type] != nullptr)
        {
            handlers[(size_t)type]->process(env, begin, end);
        }
        else
        {
            printf("Skipping processing type %d\n", (int)type);
        }
    }
};

#endif // ifndef LIBROBOCOL_PACKETPROCESSOR_H