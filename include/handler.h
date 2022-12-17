#ifndef CRCAL_HANDLER
#define CRCAL_HANDLER

#include "mutex"
#include <memory>
#include <array>
#include <queue>
#include <functional>
#include <list>

#include "packet.h"

// Usage:
// In the scope you want to be syncronized, put:
// auto l = lock();
#define SYNCHRONIZED_CLASS                           \
    Mutex accessM;                              \
    LockGuardMutex lock()               \
    {                                                \
        return {accessM}; \
    }

#define STATIC_SYNCHRONIZED_CLASS                    \
    static std::unique_ptr<Mutex> accessM;      \
    static LockGuardMutex lock()        \
    {                                                \
        if (accessM.get() == nullptr) { accessM = std::make_unique<Mutex>(); } \
        return {*accessM}; \
    }


class FixedBufItr
{
public:

    using iterator_category = std::random_access_iterator_tag;
    using value_type = char;
    using difference_type = std::ptrdiff_t;
    using pointer = char*;
    using reference = char&;

public:

    FixedBufItr(char* ptr = nullptr) { this->ptr = ptr;}
    FixedBufItr(const FixedBufItr& rawIterator) = default;
    FixedBufItr(char* ptr, char* end) { this->ptr = ptr; this->end = end; }
    ~FixedBufItr(){}

    FixedBufItr&                  operator=(const FixedBufItr& rawIterator) = default;
    FixedBufItr&                  operator=(char* ptr){ptr = ptr;return (*this);}

    operator                      bool()const
    {
        if(ptr)
            return true;
        else
            return false;
    }

    bool                                        operator==(const FixedBufItr& rawIterator)const{return (ptr == rawIterator.getConstPtr());}
    bool                                        operator!=(const FixedBufItr& rawIterator)const{return (ptr != rawIterator.getConstPtr());}

    FixedBufItr&                  operator+=(const difference_type& movement){ptr += movement;return (*this);}
    FixedBufItr&                  operator-=(const difference_type& movement){ptr -= movement;return (*this);}
    FixedBufItr&                  operator++(){++ptr;return (*this);}
    FixedBufItr&                  operator--(){--ptr;return (*this);}
    FixedBufItr                   operator++(int){auto temp(*this);++ptr;return temp;}
    FixedBufItr                   operator--(int){auto temp(*this);--ptr;return temp;}
    FixedBufItr                   operator+(const difference_type& movement){auto oldPtr = ptr;ptr+=movement;auto temp(*this);ptr = oldPtr;return temp;}
    FixedBufItr                   operator-(const difference_type& movement){auto oldPtr = ptr;ptr-=movement;auto temp(*this);ptr = oldPtr;return temp;}

    difference_type                             operator-(const FixedBufItr& rawIterator){return std::distance(rawIterator.getPtr(),this->getPtr());}

    char&                                 operator*()
    {
        if (ptr >= end)
        {
            printf("VECTOR OVERRUN %p %p\n", ptr, end);
        }
        return *ptr;
    }
    //const char&                           operator*()const{return *ptr;}
    char*                                 operator->(){return ptr;}

    char*                                 getPtr()const{return ptr;}
    const char*                           getConstPtr()const{return ptr;}

protected:

    char* ptr;

#ifndef NDEBUG
    char* end;
#endif
};


struct FixedBuf
{
    size_t len = 0;
    std::shared_ptr<char[]> buf;

    FixedBuf(size_t len)
    {
        // assert((len & (len - 1)) == 0);
        // assert(len > 0);

        printf("Making a fixedbuf of size %u\n", len);

        this->len = len;
        //buf = std::make_unique_for_overwrite<char[]>(len);
        buf = std::make_shared<char[]>(len);
    }

    FixedBuf(FixedBuf &&other)
    {
        if (len != 0 || buf != nullptr) { printf("MOVING INTO A CONSTRUCTED OBJECT\n"); }

        len = other.len;
        buf = std::move(other.buf);

        other.len = 0;
    }
    FixedBuf &operator=(FixedBuf &&other)
    {
        if (len != 0 || buf != nullptr) { printf("MOVING INTO A CONSTRUCTED OBJECT\n"); }
        len = other.len;
        buf = std::move(other.buf);

        other.len = 0;

        return *this;
    }

    /*FixedBuf(nullptr_t)
    {
        len = 0;
        buf = nullptr;
    }*/

    FixedBuf()
    {
        len = 0;
        buf = nullptr;
    }

    ~FixedBuf() 
    {
        if (len > 0 && buf != nullptr) {
        printf("FREEING A FIXEDBUF\n");}
    
    }

    // todo: remove once our compiler gains the ability to put noncopyable types in vectors??
    FixedBuf(const FixedBuf &other)
    {
        printf("Making a fixedbuf of size %u", len);
        
        this->len = other.len;
        buf = other.buf;

        /*if (other.isValid())
        {
            memcpy(buf.get(), other.buf.get(), other.len);
        }*/
    }
    FixedBuf &operator=(const FixedBuf &other)
    {
        printf("Making a fixedbuf of size %u", len);

        this->len = other.len;
        this->buf = other.buf;
        /*buf = std::make_unique<char[]>(len);

        if (other.isValid())
        {
            memcpy(buf.get(), other.buf.get(), other.len);
        }*/
        return *this;
    }

    /*FixedBuf() = default;

    FixedBuf(const FixedBuf&) = delete;
    FixedBuf& operator=(const FixedBuf&) = delete;
    FixedBuf& operator=(FixedBuf&& other)
    {
        len = other.len;
        buf = std::move(other.buf);
        return *this;
    }*/

    bool isValid() const noexcept
    {
        return len != 0 && buf.get() != nullptr;
    }

    FixedBufItr begin()
    {
        #ifndef NDEBUG
        return FixedBufItr(buf.get(), buf.get() + len);
        #else
        return FixedBufItr(buf.get());
        #endif
    }

    FixedBufItr end()
    {
        #ifndef NDEBUG
        return FixedBufItr(buf.get() + len, buf.get() + len);
        #else
        return FixedBufItr(buf.get() + len);
        #endif
    }

    char *data()
    {
        return buf.get();
    }

    size_t size()
    {
        return len;
    }
};

// Singleton storage for caching read/write buffers
class BufCache
{
public:
    static std::vector<FixedBuf> freeBufs;

    // Avoid memory leaks by storing a list of buffers that may or may not still exist
    // static std::vector<std::reference_wrapper<FixedBuf>> allBufs;

    STATIC_SYNCHRONIZED_CLASS

    // Get the smallest power of 2 equal to or greater than x
    static size_t nearestPower(size_t size)
    {
        size_t val = 1 << (32 - __builtin_clz(size - 1));

        printf("rounded %u to %u\n", size, val);

        return val;
    }

    static FixedBuf getBuf(size_t size)
    {
        assert(size < MAX_PACKET_SIZE);

        auto l = lock();

        // nearestPower ROUNDS UP
        //size_t closest = nearestPower(size);
        size_t closest = size;

        auto itr = std::find_if(freeBufs.begin(), freeBufs.end(),
                                [closest](FixedBuf &val)
                                { return val.len == closest; });

        if (itr == freeBufs.end())
        {
            FixedBuf buf(closest);
            //buf.len = size;
            return buf;
        }
        else
        {
            //todo: prevent bufs gradually losing size?
            size_t dist = std::distance(freeBufs.begin(), itr);
            printf("Moving from len %u with a dist of %u\n", freeBufs.size(), dist);
            FixedBuf buf = std::move(freeBufs[dist]);
            // freeBufs.erase(itr);
            buf.len = size;
            return buf;
        }
    }

    static void recycle(FixedBuf &&buf)
    {
        auto l = lock();

        printf("stage 6\n");

        freeBufs.emplace_back(FixedBuf());

        printf("stage 7\n");

        freeBufs.emplace_back(std::move(buf));

        printf("stage 8\n");
    }
};
std::unique_ptr<Mutex> BufCache::accessM = nullptr;
// std::vector<FixedBuf> BufCache::freeBufs = std::vector<FixedBuf>();
std::vector<FixedBuf> BufCache::freeBufs = {};

class RobocolConnection;

// Do something with a packet of a type
// Deserialize a packet of a type
class PacketHandler
{
public:
    virtual size_t process(RobocolConnection &conn, const char *begin, const char *end) = 0;

    virtual ~PacketHandler() = default;
};

// Global Robocol packet reassembler, sends completed packets to their handlers
class PacketProcessor
{
    // Functors capable of handling each type of Robocol packet
    std::array<std::unique_ptr<PacketHandler>, (size_t)MsgType::COUNT> handlers;

public:
    void addHandler(std::unique_ptr<PacketHandler> &&handler, MsgType type)
    {
        if (type >= MsgType::COUNT)
        {
            abort();
        }

        handlers.at((size_t)type) = std::move(handler);
    }

    // Defined after definition of subclasses
    void initWithDefaults();

    // Process a buffer of packets, which may contain more than one.
    void process(RobocolConnection &connection, const char *begin, const char *end)
    {
        assert(end > begin);

        MsgType type = peekType(begin, end);

        // printf("Message of type %d\n", type);

        if ((size_t)type < handlers.size() && handlers[(size_t)type] != nullptr)
        {
            handlers[(size_t)type]->process(connection, begin, end);
        }
        else
        {
            printf("Skipping processing type %d\n", (int)type);
        }
    }
};

class UdpSocket
{
public:
    int native = 0;

    sockaddr_in targetAddr = {};
    sockaddr_in bindAddr = {};

    std::deque<FixedBuf> writeQueue;
    Mutex writeQueueMutex;

    // Unconnected socket
    UdpSocket() {}

    // Create a UDP client socket listening on all interfaces
    UdpSocket(int port, const char *targetIp)
    {
        int ret = -999;

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

    void cbPoll(int events)
    {
        int ret = -999;

        if (events & (POLLERR | POLLHUP | POLLNVAL))
        {
            printf("Bad socket\n");
        }

        if (events & POLLIN)
        {
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
                printf("stage 5\n");
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
            printf("stage 1\n");
            FixedBuf ret = std::move(writeQueue.front());
            printf("stage 2\n");
            writeQueue.pop_front();
            printf("stage 3\n");
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
        if (native == -1)
        {
            net_close(native);
        }
    }

    operator int()
    {
        return native;
    }
};

// Class that pools libogc net sockets to poll them all at once
// Cannot be applicable to all files since the net_ prefix does not apply to the filesystem
class SocketPool
{
public:
    static std::vector<UdpSocket *> sockets;
    static std::vector<pollsd> polls;

    static void add(UdpSocket &sock)
    {
        if (std::find(sockets.begin(), sockets.end(), &sock) == sockets.end())
        {
            sockets.push_back(&sock);
            polls.push_back({.socket = sock, .events = POLLIN | POLLOUT, .revents = 0});
        }
    }

    static void tick()
    {
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
                UdpSocket &sock = *sockets[i];

                sock.cbPoll(poll.revents);
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
std::vector<UdpSocket *> SocketPool::sockets;
std::vector<pollsd> SocketPool::polls;

class WriteQueue // not used
{
public:
    // std::queue<FixedBuf> writeQueue;

    UdpSocket *sock;

    SYNCHRONIZED_CLASS;

    // Push a packet to the queue to be sent and later freed
    void push(FixedBuf &&buf)
    {
        auto l = lock();
        

        // writeQueue.emplace(std::move(buf));
    }

    // Take a packet for sending
    FixedBuf pop()
    {
        // if (writeQueue.size() > 0)
        {
            // FixedBuf ret = std::move(writeQueue.front());
            // writeQueue.pop();
            // return ret;
        }
        // else
        {
            return FixedBuf();
        }
    }
};

class RobocolConnection
{
public:
    // IP chosen by a REV Robot Controller when it hosts its own network - not WIFI Direct
    constexpr static const char *ROBOCOL_ROBOT_IP_DEFAULT = "192.168.43.1"; // CUSTOMIZE IN MAIN
    constexpr static uint16_t ROBOCOL_PORT_DEFAULT = 20884;

    // UDP connection socket with robot
    UdpSocket sock;

    //WriteQueue writeQueue;

    // Create default connection to robot
    RobocolConnection() : sock(ROBOCOL_PORT_DEFAULT, ROBOCOL_ROBOT_IP_DEFAULT)
    {
        SocketPool::add(sock);
        init();
    }

    RobocolConnection(const char *robotIpStr, uint16_t port = 20884) : sock(port, robotIpStr)
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
        printf("Going to write gamepad packet\n");
        FixedBuf writeBuf = BufCache::getBuf(packet.getSize());
        packet.serialize(writeBuf.begin());
        sock.write(std::move(writeBuf));
        printf("Going to write gamepad packet2\n");
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
        if (sock == -1)
        {
            net_close(sock);
        }
    }
};

class DriverStation
{
public:
    RobocolConnection robotConn;

    DriverStation(const char *robotIpStr) : robotConn(robotIpStr)
    {
        // Create and connect to a robot
    }

    void tick(int64_t delta)
    {
        robotConn.tick(delta);
    }
};

class CommandHandler : public PacketHandler
{
public:
    void sendAck(Command &command)
    {
    }

    size_t process(RobocolConnection &connection, const char *begin, const char *end)
    {
        Command packet;
        packet.parse(begin, end);

        printf("Got command for data %s and %s", packet.name.c_str(), packet.extra.data());

        connection.handle(packet);

        return (end - begin);
    }
};

void PacketProcessor::initWithDefaults()
{
    addHandler(std::make_unique<CommandHandler>(), MsgType::COMMAND);
}

#endif // ifndef CRCAL_HANDLER