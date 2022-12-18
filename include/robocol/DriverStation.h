#if !defined(LIBROBOCOL_ROBOCOL_DRIVERSTATION_H)
#define LIBROBOCOL_ROBOCOL_DRIVERSTATION_H

#include <cstdint>

#include "RobocolConnection.h"

namespace librobocol
{
    class DriverStation
    {
    public:
        RobocolConnection robotConn;

        // Create and connect to a robot
        DriverStation(const char *robotIpStr) : 
            robotConn(robotIpStr) {}

        DriverStation() : 
            robotConn() {}

        void tick(int64_t delta)
        {
            robotConn.tick(delta);
        }
    };
}

#endif // if !defined(LIBROBOCOL_ROBOCOL_DRIVERSTATION_H)