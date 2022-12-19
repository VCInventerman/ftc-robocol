#if !defined(LIBROBOCOL_ROBOCOLCONNECTION_H)
#define LIBROBOCOL_ROBOCOLCONNECTION_H

#include <cstdint>

#include "UdpSocket.h"
#include "SocketPool.h"
#include "packet.h"

namespace librobocol
{
    class RobocolConnection;
    PacketProcessor<RobocolConnection>* getRobocolPacketProcessor();

    class RobocolConnection
    {
    public:
        using MsgType = librobocol::MsgType;

        // IP chosen by a REV Robot Controller when it hosts its own network - not WIFI Direct
        constexpr static const char *ROBOCOL_ROBOT_IP_DEFAULT = "192.168.43.1"; // CUSTOMIZE IN MAIN
        constexpr static uint16_t ROBOCOL_PORT_DEFAULT = 20884;

        // UDP connection socket with robot
        UdpSocket sock;

        //WriteQueue writeQueue;

        // Create default connection to robot
        RobocolConnection() : 
            RobocolConnection(ROBOCOL_ROBOT_IP_DEFAULT, ROBOCOL_PORT_DEFAULT)
            {}

        RobocolConnection(const char *robotIpStr, uint16_t port = 20884) : 
            sock(port, robotIpStr, std::bind(&PacketProcessor<RobocolConnection>::process, getRobocolPacketProcessor(), this, std::placeholders::_1, std::placeholders::_2))
        {
            SocketPool::add(sock);
            init();
        }

        void init()
        {
            sendPeerStatus();
        }

        void sendPeerStatus()
        {
            // int ret = -999;

            PeerDiscovery packet = PeerDiscovery::forTransmission(PeerType::PEER);
            sendPacket(packet);

            //FixedBuf writeBuf = BufCache::getBuf(packet.getSize());
            // packet.serialize(writeBuf.begin());

            // ret = net_sendto(sock, writeBuf, writeBuf.size(), 0, sock.getTargetAddr(), sock.getTargetAddrSize());
            // if (ret < 0) { printf("Got error with sendto %d", errno); }
        }

        template <typename T>
        void sendPacket(T &packet)
        {
            printf("Going to write packet of type %s\n", typeid(packet).name());
            FixedBuf writeBuf = BufCache::getBuf(packet.getSize());
            size_t written = packet.serialize(writeBuf.begin());
            sock.write(std::move(writeBuf));
            printf("Wrote packet, size %d\n", written);
        }

        void tick(int64_t delta)
        {
            sock.tick(delta);
        }

        // Do something with a deserialized packet
        template <typename PacketT>
        void handle(PacketT &packet)
        {
            assert(false);
        }

        template <typename PacketT>
        void handle(Command &packet)
        {
            printf("Looking at a command");
        }

        ~RobocolConnection()
        {
            
        }

        // Get the type of a packet without advancing the iterator
        template <typename ItrT>
        size_t peekType(ItrT begin, ItrT end)
        {
            MsgType type = MsgType::EMPTY;

            // read(begin, end, type);
            type = (MsgType)*begin;

            return (size_t)type;
        }
    };
}

#endif // if !defined(LIBROBOCOL_ROBOCOLCONNECTION_H)