#include <mutex>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <debug.h>
#include <errno.h>
#include <wiiuse/wpad.h>

#include <fcntl.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <system_error>

#include <debug/vector>

#include "robocol/DriverStation.h"
#include "robocol/handlers.h"

using namespace librobocol;

struct local_inet_ntop
{
	static constexpr size_t IP_STR_SIZE = 16;

	char str[IP_STR_SIZE] = {0};

	local_inet_ntop(in_addr &addr)
	{
		// inet_ntoa creates an ip string in a static buffer lost after the next call to it
		char *staticStr = inet_ntoa(addr);
		memcpy(str, staticStr, IP_STR_SIZE);
	}

	operator char *()
	{
		return str;
	}
};

std::pair<GXRModeObj *, void *> initVideo()
{
	GXRModeObj *renderMode = nullptr;
	void *framebuffer = nullptr;

	VIDEO_Init();

	renderMode = VIDEO_GetPreferredMode(NULL);
	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(renderMode));
	//console_init(framebuffer, 20, 20, renderMode->fbWidth, renderMode->xfbHeight, renderMode->fbWidth * VI_DISPLAY_PIX_SZ);
	CON_InitEx(renderMode, 20, 20, renderMode->fbWidth, 100);

	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(0, renderMode->fbWidth, renderMode->xfbHeight);

	VIDEO_Configure(renderMode);
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (renderMode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	return {renderMode, framebuffer};
}

void initNetwork()
{
	in_addr localip = {0};
	memset(&localip, 0, sizeof(localip));
	in_addr netmask = {0};
	memset(&netmask, 0, sizeof(netmask));
	in_addr gateway = {0};
	memset(&gateway, 0, sizeof(gateway));

	int ret;
	ret = if_configex(&localip, &netmask, &gateway, true, 20);
	if (ret >= 0)
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wformat"
		printf("network configured, ip: %s, gw: %s, mask %s\n", local_inet_ntop(localip), local_inet_ntop(gateway), local_inet_ntop(netmask));
		#pragma GCC diagnostic pop
	}
	else
	{
		printf("network configuration failed!\n");
	}
}

lwp_t netFlowHandle = lwp_t(0);
void *netFlow(void *packetHandlerPtr)
{
	// PacketProcessor& packetHandler = *(PacketProcessor*)packetHandlerPtr;

	while (true)
	{

		usleep(100);
	}

	return nullptr;
}

int main(int argc, char *argv[])
{
	try 
	{
		printf("Opened\n");

		/*auto [ renderMode, framebuffer ] = */ initVideo();

		initNetwork();

		DriverStation station("192.168.43.164");

		PacketProcessor processor = getRobocolPacketProcessor();
		

		int64_t time = currentTimeNs();
		int64_t deltaTime = 0;
		int64_t videoRefreshDeltaTime = 0;

		int64_t controllerRefreshDeltaTime = 0;
		GamepadPacket prevGamepadState = GamepadPacket::fromWiimote(0);

		while (1)
		{
			int64_t currentTimeCache = currentTimeNs();
			deltaTime = currentTimeCache - time;
			time = currentTimeCache;

			SocketPool::tick();
			station.tick(deltaTime);
			//printf("alive\n");

			videoRefreshDeltaTime += deltaTime;
			if (videoRefreshDeltaTime > 1666667)
			{
				VIDEO_WaitVSync();
				videoRefreshDeltaTime = 0;
			}

			WPAD_ScanPads();

			usleep(30);

			uint32_t buttonsDown = WPAD_ButtonsDown(0);
			if (buttonsDown & WPAD_BUTTON_HOME)
			{
				exit(0);
			}

			controllerRefreshDeltaTime += deltaTime;
			if (controllerRefreshDeltaTime > 100'000'000 /* .1s */)
			{
				GamepadPacket newPacket = GamepadPacket::fromWiimote(0);
				if (newPacket != prevGamepadState)
				{
					station.robotConn.sendPacket(newPacket);
					prevGamepadState = newPacket;
				}

				controllerRefreshDeltaTime = 0;
			}
		}



	}
	catch (std::system_error& e) 
	{
		printf("System Error: %d, %d, %s, %s\n", e.code().value(), errno, strerror(errno), strerror(e.code().value()));
		printf("General Exception: %d\n", e.code().value());
	}
	catch (std::exception& e) 
	{
		printf("General Exception: %s %s\n", e.what(), typeid(e).name());
		printf("General Exception: %s\n", e.what());
	}
	catch (std::error_code e) 
	{
		printf("General Error Code: %s\n", e.message().data());
	}
}