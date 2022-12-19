#if !defined(LIBROBOCOL_UDPSOCKET_H)
#define LIBROBOCOL_UDPSOCKET_H

#include <deque>
#include <network.h>

#include "sync.h"
#include "FixedBuf.h"
#include "BufCache.h"
#include "Socket.h"
#include "PacketProcessor.h"


namespace librobocol
{
    class UdpSocket : public LibogcNetSocket
    {
    public:
        int native = 0;

        sockaddr_in targetAddr = {};
        sockaddr_in bindAddr = {};

        std::deque<FixedBuf> writeQueue;
        Mutex writeQueueMutex;

        std::unique_ptr<char[]> readBuf;
        size_t readBufSize = 66000;

        std::function<void(char*, char*)> processor;

        // Unconnected socket
        UdpSocket()
        {
            readBuf = std::make_unique<char[]>(66000);
        }

        // Create a UDP client socket listening on all interfaces
        UdpSocket(int port, const char *targetIp, std::function<void(char*, char*)> processorFunc)
        {
            int ret = -999;

            processor = processorFunc;

            readBuf = std::make_unique<char[]>(66000);

            printf("Opening a socket on %s:%d", targetIp, port);

            // Robot IP
            targetAddr = {.sin_len = sizeof(targetAddr), .sin_family = AF_INET, .sin_port = (u16)htons(port)};
            if ((targetAddr.sin_addr.s_addr = inet_addr(targetIp)) == 0)
            {
                fprintf(stderr, "inet_aton() failed\n");
            }

            // Match all IPs on port (0.0.0.0)
            bindAddr = {.sin_len = sizeof(bindAddr), .sin_family = AF_INET, .sin_port = (u16)htons(port)};
            //bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            bindAddr.sin_addr.s_addr = inet_addr(targetIp);

            // Create UDP socket
            native = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
            if (native == INVALID_SOCKET || native < 0)
            {
                printf("Cannot create a socket!\n");
            }

            
            printf("Target:%s", inet_ntoa(*(in_addr*)&bindAddr.sin_addr));

            // Bind socket for packets to be sent back to it
            //ret = net_bind(native, (sockaddr *)&bindAddr, sizeof(bindAddr));
            if (ret < 0)
            {
                //perror("Failed to bind");
            }

            // Set nonblocking
            // ret = net_fcntl(sock, F_SETFL, net_fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
            // if (ret == -1) { printf("Cannot set nonblocking"); }
        }

        void handlePollResult(int events)
        {
            int ret = -999;

            if (events & (POLLERR | POLLHUP | POLLNVAL))
            {
                printf("Bad socket\n");
            }

            if (events & POLLIN)
            {
                u32 readBytes = getTargetAddrSize();
                ret = net_recvfrom(native, readBuf.get(), readBufSize, 0, getTargetAddr(), &readBytes);
                if (ret < 0)
                {
                    printf("recvfrom error %d\n", ret);
                }
                else
                {
                    processor(readBuf.get(), readBuf.get() + readBytes);
                }
            }

            if (events & POLLOUT)
            {
                FixedBuf buf = pop();

                if (buf.size() > 0)
                {
                    printf("Sending with a size of %u\n", buf.size());
                    ret = net_sendto(native, buf.data(), buf.size(), 0, getTargetAddr(), getTargetAddrSize());
                    if (ret < 0)
                    {
                        printf("Got error with sendto %d", errno);
                    }

                    BufCache::recycle(std::move(buf));
                }
            }

            // if (events &)
        }

        /*template <typename CallbackT>
        void poll(CallbackT cbRead, CallbackT cbWrite, CallbackT cbError)
        {
            pollsd pollinfo = { .socket = sock, .events = POLLIN | POLLOUT, .revents = 0 };
            net_poll(&pollinfo, 1, 0);
        }*/

        // Push a packet to the queue to be sent and later freed
        void write(FixedBuf &&buf)
        {
            LockGuardMutex lock(writeQueueMutex);
            writeQueue.emplace_back(std::move(buf));
        }

        // Take a packet for sending
        FixedBuf pop()
        {
            LockGuardMutex lock(writeQueueMutex);

            if (writeQueue.size() > 0)
            {
                FixedBuf ret = std::move(writeQueue.front());
                writeQueue.pop_front();
                return ret;
            }
            else
            {
                return FixedBuf();
            }
        }

        // Handled by SocketPool
        void tick(int64_t delta)
        {
        }

        sockaddr *getTargetAddr()
        {
            return (sockaddr *)&targetAddr;
        }

        size_t getTargetAddrSize()
        {
            return sizeof(targetAddr);
        }

        sockaddr *getBindAddr()
        {
            return (sockaddr *)&bindAddr;
        }

        size_t getBindAddrSize()
        {
            return sizeof(bindAddr);
        }

        ~UdpSocket()
        {
            if (native > 0)
            {
                net_close(native);
            }
        }

        int getSocketHandle() const noexcept
        {
            return native;
        }
    };
}

#endif // if !defined(LIBROBOCOL_UDPSOCKET_H)