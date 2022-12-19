#if !defined(LIBROBOCOL_ROBOCOL_PACKET_H)
#define LIBROBOCOL_ROBOCOL_PACKET_H

#include <chrono>
#include <variant>
#include <atomic>
#include <cassert>
#include <algorithm>
#include <time.h>

#ifdef GEKKO
#include <wiiuse/wpad.h>
#include <lwp_watchdog.h>
#endif

#ifndef BIGENDIAN
#define BIGENDIAN 0
#define LITTLEENDIAN 1
#endif

#if defined(PPC)
#define HOST_ENDIAN BIGENDIAN
#else
#define HOST_ENDIAN LITTLEENDIAN
#endif
#define NETWORK_ENDIAN BIGENDIAN

namespace librobocol
{

    constexpr char ROBOCOL_VERSION = 123;
    constexpr char SDK_BUILD_MONTH = 9;
    constexpr int16_t SDK_BUILD_YEAR = 2022;
    constexpr char SDK_MAJOR_VERSION = 8;
    constexpr char SDK_MINOR_VERSION = 0;

    constexpr size_t MAX_PACKET_SIZE = 55000;

    enum class RobotState : int8_t
    {
        UNKNOWN = -1,
        NOT_STARTED = 0,
        INIT = 1,
        RUNNING = 2,
        STOPPED = 3,
        EMERGENCY_STOP = 4
    };

    enum class MsgType : char
    {
        EMPTY = 0,
        HEARTBEAT = 1,
        GAMEPAD = 2,
        PEER_DISCOVERY = 3,
        COMMAND = 4,
        TELEMETRY = 5,
        KEEPALIVE = 6,
        COUNT
    };

    enum class PeerType : char
    {
        NOT_SET = 0,
        PEER = 1,
        GROUP_OWNER [[deprecated]] = 2,
        NOT_CONNECTED_DUE_TO_PREEXISTING_CONNECTION = 3
    };

    template <typename T>
    T swap_endian(T u)
    {
        union
        {
            T u;
            unsigned char u8[sizeof(T)];
        } source, dest;

        source.u = u;

        for (size_t k = 0; k < sizeof(T); k++)
            dest.u8[k] = source.u8[sizeof(T) - k - 1];

        return dest.u;
    }




    int64_t currentTimeNs()
    {
        /*return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
            */

        

        u64 val = ticks_to_nanosecs(gettime());
        //printf("Got ns %llu\n", val);
        return val;
    }

    template <typename DataT, typename OutT>
    size_t emit(const DataT &data, OutT &out, int endian = NETWORK_ENDIAN)
    {
        char bytes[sizeof(data)];
        memcpy(bytes, &data, sizeof(data));

        size_t written = 0;

        bool switchEndian = (HOST_ENDIAN != endian);

        if (switchEndian)
        {
            std::reverse_copy(bytes, bytes + sizeof(DataT), out);
            written = sizeof(DataT);

            /*for (auto itr = std::rbegin(bytes); itr != std::rend(bytes); itr++)
            {
                char byte = *itr;
                out = byte;
                out++;
                written++;
            }*/
        }
        else
        {
            for (auto itr = std::begin(bytes); itr != std::end(bytes); itr++)
            {
                char byte = *itr;
                *out = byte;
                out++;
                written++;
            }
        }

        return written;
    }

    // Read from a buffer in network byte order to a host order slot. Advance iterator begin to past last byte read.
    template <typename ItrT, typename DataT>
    typename std::enable_if<!std::is_enum<DataT>::value, size_t>::type
    read(ItrT &begin, const ItrT end, DataT &slot, int endian = NETWORK_ENDIAN)
    {
        union SlotBufT
        {
            DataT orig;
            char bytes[sizeof(DataT)];
        };

        SlotBufT &var = *(SlotBufT *)&slot;

        size_t readBytes = 0;

        bool switchEndian = (HOST_ENDIAN != endian);

        if (switchEndian)
        {
            std::reverse_copy(begin, begin + sizeof(DataT), var.bytes);
            readBytes = sizeof(DataT);
        }
        else
        {
            std::copy(begin, begin + sizeof(DataT), var.bytes);
            readBytes = sizeof(DataT);
        }

        begin += readBytes;

        return readBytes;
    }

    template <typename ItrT, typename DataT>
    typename std::enable_if<std::is_enum<DataT>::value, size_t>::type
    read(ItrT &begin, const ItrT end, DataT &slot, int endian = NETWORK_ENDIAN)
    {
        typename std::underlying_type<DataT>::type buf{};
        size_t bytesRead = read(begin, end, buf, endian);
        slot = (DataT)buf;
        return bytesRead;
    }

    #pragma pack(push, 1)
    struct PacketHeader
    {
        MsgType type;
        uint16_t payloadLength;
        uint16_t sequenceNum;

        template <typename OutT>
        size_t emit(OutT &out)
        {
            size_t written = 0;
            written += librobocol::emit(type, out);
            written += librobocol::emit(payloadLength, out);
            written += librobocol::emit(sequenceNum, out);
            return written;
        }
    };

    struct RawPacket
    {
        PacketHeader header;

        char payload[];
    };
    #pragma pack(pop)

    class PacketCommon
    {
    public:
        static std::atomic_uint16_t nextSequenceNum;
    };

    template <typename PacketImplT>
    class Packet
    {
    protected:
        uint16_t sequenceNum = 0;
        int64_t nanotimeTransmit = 0;

        static constexpr long nanotimeTransmitInterval = 200LL * 1000000LL;

        Packet(uint16_t sequenceNum)
        {
            this->sequenceNum = sequenceNum;
            nanotimeTransmit = 0;
        }

        Packet()
        {
            this->sequenceNum = PacketCommon::nextSequenceNum++;
            nanotimeTransmit = 0;
            assert(PacketCommon::nextSequenceNum < 40000);
        }

        template <typename ItrT>
        Packet(ItrT &begin, ItrT &end)
        {
            char type = MsgType::EMPTY;
            uint16_t payloadLength = 0;
            sequenceNum = 0;

            read(begin, end, type);
            read(begin, end, payloadLength);
            read(begin, end, sequenceNum);
        }

    public:
        auto operator<=>(const Packet<PacketImplT> &) const = default;

        template <typename OutT>
        size_t serialize(OutT &out)
        {
            return ((PacketImplT *)this)->serializeImpl(out);
        }

        template <typename OutT>
        size_t serialize(OutT &&out)
        {
            return serialize(out);
        }

        template <typename OutT>
        size_t serializeForTransmit(OutT &out)
        {
            size_t written = serialize(out);
            nanotimeTransmit = currentTimeNs();
            return written;
        }

        template <typename OutT>
        size_t serializeForTransmit(OutT &&out)
        {
            size_t written = serialize(out);
            nanotimeTransmit = currentTimeNs();
            return written;
        }

        bool shouldTransmit(long nanotimeNow)
        {
            return this->nanotimeTransmit == 0 || (nanotimeNow - this->nanotimeTransmit > nanotimeTransmitInterval);
        }
    };
    std::atomic_uint16_t PacketCommon::nextSequenceNum = 10000;

    class Heartbeat : public Packet<Heartbeat>
    {
        int64_t timestamp;
        RobotState robotState;
        int64_t t0;
        int64_t t1;
        int64_t t2;

        std::string timeZoneId;

    public:
        Heartbeat()
        {
            timestamp = 0;
            robotState = RobotState::NOT_STARTED;
            t0 = t1 = t2 = 0;
            timeZoneId = "America/Chiicago";
        }

        static Heartbeat createWithTimeStamp()
        {
            Heartbeat result;
            result.timestamp = currentTimeNs();
            return result;
        }

        template <typename OutT>
        size_t serializeImpl(OutT &out)
        {
            size_t written = 0;

            size_t payloadLength = 8 + 1 + 8 + 8 + 8 + 1 + timeZoneId.size();
            written += (PacketHeader{MsgType::HEARTBEAT, (uint16_t)payloadLength, sequenceNum}).emit(out);

            written += emit(timestamp, out);
            written += emit(robotState, out);
            written += emit(t0, out);
            written += emit(t1, out);
            written += emit(t2, out);
            written += emit((char)timeZoneId.size(), out);
            out = std::copy(timeZoneId.begin(), timeZoneId.end(), out);
            written += timeZoneId.size();

            return written;
        }
    };

    class PeerDiscovery : public Packet<PeerDiscovery>
    {
        PeerType peerType;

        char sdkBuildMonth;
        int16_t sdkBuildYear;
        int32_t sdkMajorVersion;
        int32_t sdkMinorVersion;

        static constexpr int cbBufferHistorical = 13;
        static constexpr int16_t cbPayloadHistorical = 10;

    public:
        PeerDiscovery(PeerType peerType, char sdkBuildMonth, short sdkBuildYear, int sdkMajorVersion, int sdkMinorVersion)
        {
            this->peerType = peerType;
            this->sdkBuildMonth = sdkBuildMonth;
            this->sdkBuildYear = sdkBuildYear;
            this->sdkMajorVersion = sdkMajorVersion;
            this->sdkMinorVersion = sdkMinorVersion;
        }

        static PeerDiscovery forReceive()
        {
            return PeerDiscovery(PeerType::NOT_SET, (char)1, (int16_t)1, 0, 0);
        }

        static PeerDiscovery forTransmission(PeerType peerType)
        {
            return PeerDiscovery(peerType, SDK_BUILD_MONTH, SDK_BUILD_YEAR, SDK_MAJOR_VERSION, SDK_MINOR_VERSION);
        }

        //  1 byte    message type
        //  2 bytes   payload size (big endian)
        //  1 byte    ROBOCOL_VERSION
        //  1 byte    peer type
        //  2 bytes   sequence number (big endian)
        //  1 byte    month that the SDK version was released in (1-12)
        //  2 bytes   year that the SDK version was released in (big endian)
        //  1 byte    major SDK version number (unsigned)
        //  1 byte    minor SDK version number (unsigned)
        //  1 byte    ignored
        template <typename OutT>
        size_t serializeImpl(OutT &out)
        {
            size_t written = 0;

            sequenceNum = 10007;

            written += emit(MsgType::PEER_DISCOVERY, out);
            written += emit(cbPayloadHistorical, out);
            written += emit(ROBOCOL_VERSION, out);
            written += emit(peerType, out);
            written += emit(sequenceNum, out);
            written += emit(sdkBuildMonth, out);
            written += emit(sdkBuildYear, out);
            written += emit((char)sdkMajorVersion, out);
            written += emit((char)sdkMinorVersion, out);
            written += emit((char)0, out);

            assert(written == 13);
            return written;
        }

        size_t getSize()
        {
            return 13;
        }
    };

    class Command : public Packet<Command>
    {
    public:
        std::string name;
        std::string extra;
        int64_t timestamp;
        bool acknowledged = false;
        char attempts = 0;
        bool isInjected = false; // not transmitted over network
        int64_t transmissionDeadlineNs;

        // space for the timestamp (8 bytes), ack byte (1 byte)
        static constexpr int16_t cbStringLength = 2;
        static constexpr int16_t cbPayloadBase = 8 + 1;

    public:
        /*template<typename ItrT>
        Command(ItrT begin, ItrT end)
        {
            assert(std::distance(begin, end) > cbPayloadBase);


        }*/

        Command(std::string &&name, std::string &&extra)
        {
            this->name = name;
            this->extra = extra;

            timestamp = currentTimeNs();
            sequenceNum = PacketCommon::nextSequenceNum++;
        }

        Command() {}

        auto operator<=>(const Command &) const = default;

        static int getPayloadSize(bool acknowledged, int nameBytesLength, int extraBytesLength)
        {
            if (acknowledged)
            {
                return cbPayloadBase + cbStringLength + nameBytesLength;
            }
            else
            {
                return cbPayloadBase + cbStringLength + nameBytesLength + cbStringLength + extraBytesLength;
            }
        }

        int getSize()
        {
            return 5 + getPayloadSize(acknowledged, name.size(), extra.size());
        }

        template <typename OutT>
        size_t serializeImpl(OutT &out)
        {
            size_t written = 0;

            size_t payloadLength = getPayloadSize(acknowledged, name.size(), extra.size());
            written += (PacketHeader{MsgType::COMMAND, (uint16_t)payloadLength, sequenceNum}).emit(out);

            written += emit(timestamp, out);
            written += emit(acknowledged, out);
            written += emit((int16_t)name.size(), out);
            out = std::copy(name.begin(), name.end(), out);
            written += name.size();

            // If we are just an ack, then we don't transmit the body in order to save net bandwidth
            if (!acknowledged)
            {
                written += emit((int16_t)extra.size(), out);
                out = std::copy(extra.begin(), extra.end(), out);
                written += extra.size();
            }

            return written;
        }

        template <typename ItrT>
        ItrT parse(ItrT begin, ItrT end)
        {
            size_t bytesLeft = end - begin;

            MsgType type = MsgType::EMPTY;
            uint16_t payloadLength = 0;

            assert(end - begin > 5);

            read(begin, end, type);
            assert(type == MsgType::COMMAND);

            read(begin, end, payloadLength);
            read(begin, end, sequenceNum);

            read(begin, end, timestamp);
            read(begin, end, acknowledged);

            uint16_t nameLength = 0;
            read(begin, end, nameLength);
            assert(nameLength < 1000 && nameLength < bytesLeft);

            name.resize(nameLength, '\0');
            std::copy(begin, end, name.data());
            begin += nameLength;

            if (!acknowledged)
            {
                uint16_t extraLength = 0;
                read(begin, end, extraLength);

                extra.resize(extraLength, '\0');
                std::copy(begin, end, extra.data());
                begin += extraLength;
            }

            return begin;
        }
    };

    class GamepadPacket : public Packet<GamepadPacket>
    {
    public:
        static constexpr size_t PAYLOAD_SIZE = 60;
        static constexpr size_t BUFFER_SIZE = PAYLOAD_SIZE + 5 /*RobocolParsable.HEADER_LENGTH*/;

        static constexpr uint8_t ROBOCOL_GAMEPAD_VERSION = 5;
        static constexpr int32_t DEFAULT_CONTROLLER_ID = -1;

        enum LegacyType : uint32_t {  UNKNOWN,  LOGITECH_F310,  XBOX_360,  SONY_PS4 };



        float left_stick_x = 0.0f;
        float left_stick_y = 0.0f;
        float right_stick_x = 0.0f;
        float right_stick_y = 0.0f;
        float left_trigger = 0.0f;
        float right_trigger = 0.0f;
        float touchpad_finger_1_x = 0.0f;
        float touchpad_finger_1_y = 0.0f;
        int32_t buttons = 0;

        int32_t id = -1;
        uint8_t user = 1; // 1 or 2



        GamepadPacket() {}

        template <typename OutT>
        size_t serializeImpl(OutT &out)
        {
            size_t written = 0;

            size_t payloadLength = BUFFER_SIZE - 5;
            written += (PacketHeader{MsgType::GAMEPAD, (uint16_t)payloadLength, sequenceNum}).emit(out);
            printf("starting %d ", written);

            written += emit(ROBOCOL_GAMEPAD_VERSION, out);
            written += emit(id, out);
            written += emit(currentTimeNs(), out); // timestamp
            written += emit(left_stick_x, out); // left_stick_x
            written += emit(left_stick_y, out); // left_stick_y
            written += emit(right_stick_x, out); // right_stick_x
            written += emit(right_stick_y, out); // right_stick_y
            written += emit(left_trigger, out); // left_trigger
            written += emit(right_trigger, out); // right_trigger
            written += emit(buttons, out);   // buttons
            printf("%d ", written);


            written += emit(user, out);
            written += emit((uint8_t)LegacyType::LOGITECH_F310, out);
            written += emit((uint8_t)LegacyType::LOGITECH_F310, out);
            printf("%d", written);


            written += emit(touchpad_finger_1_x, out); // touchpad_finger_1_x
            written += emit(touchpad_finger_1_y, out); // touchpad_finger_1_y
            written += emit(0.0f, out); // touchpad_finger_2_x
            written += emit(0.0f, out); // touchpad_finger_2_y

            printf("%d\n", written);


            return written;
        }

    #ifdef GEKKO
        static GamepadPacket fromWiimote(int channel)
        {
            GamepadPacket packet;

            WPADData* controller = WPAD_Data(channel);

            int32_t& buttons = packet.buttons;
            buttons = (buttons << 1); // + (touchpad_finger_1 ? 1 : 0);
            buttons = (buttons << 1); // + (touchpad_finger_2 ? 1 : 0);
            buttons = (buttons << 1); // + (touchpad ? 1 : 0);
            buttons = (buttons << 1); // + (left_stick_button ? 1 : 0);
            buttons = (buttons << 1); // + (right_stick_button ? 1 : 0);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_UP);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_DOWN);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_LEFT);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_RIGHT);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_A);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_B);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_1);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_2);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_HOME);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_PLUS);
            buttons = (buttons << 1) + bool(controller->btns_h & WPAD_BUTTON_MINUS);
            buttons = (buttons << 1); // + (left_bumper ? 1 : 0);
            buttons = (buttons << 1); // + (right_bumper ? 1 : 0);


            if (controller->ir.valid)
            {
                //packet.touchpad_finger_1_x = controller->ir.x;
                //packet.touchpad_finger_1_y = controller->ir.y;
                // Angle and other orientation data not included
            }
            else
            {
                packet.touchpad_finger_1_x = 0.0f;
                packet.touchpad_finger_1_y = 0.0f;
            }

            if (controller->exp.type == WPAD_EXP_NUNCHUK)
            {
                packet.left_stick_x = controller->exp.nunchuk.js.pos.x;
                packet.left_stick_y = controller->exp.nunchuk.js.pos.y;
            }

            return packet;
        }

        bool operator==(GamepadPacket& lhs)
        {
            return left_stick_x == lhs.left_stick_x &&
                left_stick_y == lhs.left_stick_y &&
                right_stick_x == lhs.right_stick_x &&
                right_stick_y == lhs.right_stick_y &&
                left_trigger == lhs.left_trigger &&
                right_trigger == lhs.right_trigger &&
                touchpad_finger_1_x == lhs.touchpad_finger_1_x &&
                touchpad_finger_1_y == lhs.touchpad_finger_1_y &&
                buttons == lhs.buttons &&
                id == lhs.id &&
                user == lhs.user;
        }

        size_t getSize()
        {
            return PAYLOAD_SIZE + 5;
        }
    };
    #endif

    class AnyPacket
    {
        std::variant<Heartbeat, PeerDiscovery, Command> store;
    };
}

#endif // if !defined(LIBROBOCOL_ROBOCOL_PACKET_H)