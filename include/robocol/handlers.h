#if !defined(LIBROBOCOL_ROBOCOL_HANDLERS_H)
#define LIBROBOCOL_ROBOCOL_HANDLERS_H

#include <memory>

#include "PacketProcessor.h"
#include "packet.h"
#include "RobocolConnection.h"

namespace librobocol
{
    class CommandHandler : public PacketHandler<RobocolConnection>
    {
    public:
        void sendAck(Command &command, RobocolConnection& connection)
        {
            Command ack = command;
            ack.acknowledged = true;

            connection.sendPacket(ack);
        }

        size_t process(RobocolConnection* connection, const char *begin, const char *end)
        {
            Command packet;
            packet.parse(begin, end);

            //printf("Got command for data %s and %s", packet.name.c_str(), packet.extra.data());

            //connection->handle<Command>(packet);

            // Send the acknowledgement back if needed
            if (packet.acknowledged == false)
            {
                packet.acknowledged = false;
                //FixedBuf ack = BufCache::getBuf(packet.getSize());
                //packet.serializeForTransmit(ack.begin());
                //connection->sendPacket(packet);
            }

            return (end - begin);
        }
    };

    PacketProcessor<RobocolConnection>* getRobocolPacketProcessor()
    {
        static PacketProcessor<RobocolConnection> processor;

        if (processor.packetTypeCount() == 0)
        {
            //todo: telemetry
            processor.addHandler(std::make_unique<CommandHandler>(), MsgType::COMMAND);
        }

        return &processor;
    }
}

#endif // if !defined(LIBROBOCOL_ROBOCOL_HANDLERS_H)