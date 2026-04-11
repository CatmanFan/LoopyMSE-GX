#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <climits>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gccore.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#ifdef USE_WIIDRC
#include "wiidrc/wiidrc.h"
#endif
#include <asndlib.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <SDL2/SDL.h>
#include "common/bswp.h"
#include "core/config.h"
#include "core/loopy_io.h"
#include "core/system.h"
#include "input/input.h"
#include "sound/sound.h"
#include "video/video.h"

namespace SDL
{
	using Video::DISPLAY_HEIGHT;
	using Video::DISPLAY_WIDTH;

	struct Screen
	{
		SDL_Window* window;
		SDL_Renderer* renderer;
		SDL_Texture* texture;
	};

	static Screen screen;

	bool initialize()
	{
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
		{
			printf("SDL2 error: %s\n", SDL_GetError());
			return false;
		}

		//Try synchronizing drawing to VBLANK
		// SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

		// make sure SDL cleans up before exit
		atexit(SDL_Quit);
		SDL_ShowCursor(SDL_DISABLE);

		// create a new window
		screen.window = SDL_CreateWindow(
			"Rupi",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			640, 480,
			0
		);
		if (!screen.window) {
			printf("SDL_CreateWindow error: %s\n", SDL_GetError());
			return false;
		}
		screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_ACCELERATED);
		if (!screen.renderer) {
			printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
			return false;
		}

		screen.texture = SDL_CreateTexture(screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
		return true;
	}

	void shutdown() {
		SDL_SetRenderDrawColor(screen.renderer, 0, 0, 0, 255);
		SDL_RenderClear(screen.renderer);
		SDL_RenderPresent(screen.renderer);
		VIDEO_SetBlack(true);

		//Destroy window, then kill SDL2
		SDL_DestroyTexture(screen.texture);
		SDL_DestroyRenderer(screen.renderer);
		SDL_DestroyWindow(screen.window);

		SDL_Quit();
	}

	void update(uint16_t* display_output)
	{
		// More efficient alternative to SDL_UpdateTexture(screen.texture, NULL, display_output, sizeof(uint16_t) * DISPLAY_WIDTH);
		void* pixels;
		int pitch;
		if (SDL_LockTexture(screen.texture, nullptr, &pixels, &pitch) == 0)
		{
			memcpy(pixels, display_output, sizeof(uint16_t) * DISPLAY_WIDTH * DISPLAY_HEIGHT);
			SDL_UnlockTexture(screen.texture);
		}

		// Clear screen
		SDL_SetRenderDrawColor(screen.renderer, 15, 15, 15, 255);
		SDL_RenderClear(screen.renderer);

		// Draw screen
		// SDL_Rect dest = {640 / 2 - (DISPLAY_WIDTH / 2), 480 / 2 - (DISPLAY_HEIGHT / 2), DISPLAY_WIDTH, DISPLAY_HEIGHT};
		SDL_RenderCopy(screen.renderer, screen.texture, NULL, NULL);
		SDL_RenderPresent(screen.renderer);
	}
}

static std::string devicePrefix;

static std::string remove_extension(std::string file_path)
{
	auto pos = file_path.find(".");
	if (pos == std::string::npos)
		return file_path;

	return file_path.substr(0, pos);
}

static bool initFat() {
	if (!fatInitDefault()) {
		printf("fatInitDefault failure: terminating\n");
		return false;
	}

	if(fatMountSimple("sd", &__io_wiisd)) {
		printf("detected SD\n");
		devicePrefix = "sd:/";
	} else if (fatMountSimple("usb", &__io_usbstorage)){
		printf("detected USB\n");
		devicePrefix = "usb:/";
	} else {
		printf("fatMountSimple failure: terminating\n");
		return false;
	}

	DIR *pdir = opendir(devicePrefix.c_str());
	if (!pdir){
		printf ("opendir() failure; terminating\n");
		return false;
	}
	closedir(pdir);

	return true;
}

static bool shutdown = false, reset = false;

static void cbShutdown() { shutdown = true; }
static void cbShutdownWpad(s32 chan) { shutdown = true; }
static void cbReset(u32 chan, void* arg) { reset = true; }

static void fatal(const char *txt)
{
	perror("HALT");
	printf("      %s, exiting in 5 seconds\n", txt);
	sleep(5);
	exit(0);
}

static std::string appPath;
static void CreateAppPath(std::string file_path)
{
	auto pos = file_path.find(".");
	if (pos == std::string::npos)
		appPath = "sd:/apps/LoopyMSE-GX";
	else
		appPath = file_path.substr(0, pos - 4);
}

static void PrintHeader()
{
    printf("\033[2J\033[H"); // Clear screen
	printf("LoopyMSE-GX v0.1 (experimental)                 Casio Loopy emulator for Wii\n");
	printf("____________________________________________________________________________\n\n");
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	PAD_Init();
	SYS_SetPowerCallback(cbShutdown);
	SYS_SetResetCallback(cbReset);

	#ifdef HW_RVL
	WPAD_Init();
	// Enable all buttons and accelerometer for all connected controllers
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetPowerButtonCallback(cbShutdownWpad);
	#endif

	#ifdef USE_WIIDRC
	WiiDRC_Init();
	bool hasDRC = WiiDRC_Inited() && WiiDRC_Connected();
	#endif

	// Initialise the console
	GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
	void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	PrintHeader();
	printf("Loading\n");

	if (!initFat()) {
		fatal("failed to init libfat");
	}

	// store path app was loaded from
	CreateAppPath(argc > 0 && argv[0] != NULL ? argv[0] : "sd:/apps/LoopyMSE-GX/boot.dol");
	printf("Running from %s\n", appPath.c_str());

	Config::SystemInfo config;
	std::string bios_name = appPath + "/bios.bin";
	std::string sound_rom_name = appPath + "/soundbios.bin";

	std::ifstream bios_file(bios_name, std::ios::binary);
	if (!bios_file.is_open())
	{
		fatal("bios.bin not found");
	}
	config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
	bios_file.close();

	// If last argument is given, load the sound ROM
	std::ifstream sound_rom_file(sound_rom_name, std::ios::binary);
	if (!sound_rom_file.is_open())
	{
		printf("Warning: soundbios.bin not found.\n         Emulator will continue without sound.");
		sleep(3);
	}
	config.sound_rom.assign(std::istreambuf_iterator<char>(sound_rom_file), {});
	sound_rom_file.close();

	// Look for ROMs in directory
	std::vector<std::string> roms;
	unsigned int rom_selected = 0;

	if (!std::filesystem::is_directory(appPath + "/roms"))
	{
		fatal("roms directory not found");
	}

    for (const auto & rom : std::filesystem::directory_iterator(appPath + "/roms")) {
        if (rom.path().extension().compare(".bin") == 0)
			roms.push_back(rom.path().stem());
	}

	if (roms.size() == 0)
	{
		fatal("no ROMs found in roms directory");
	}

	bool inMenu = true;
	bool render = true;

	while (SYS_MainLoop() && inMenu)
	{
		PAD_ScanPads();

		#ifdef HW_RVL
		WPAD_ScanPads();
		#ifdef USE_WIIDRC
			WiiDRC_ScanPads();
			const struct WiiDRCData *drcdat = WiiDRC_Data();
			if ((hasDRC && drcdat->button & WIIDRC_BUTTON_UP) || WPAD_ButtonsDown(0) & WPAD_BUTTON_UP || PAD_ButtonsDown(0) & PAD_BUTTON_UP) {
		#else
			if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP || PAD_ButtonsDown(0) & PAD_BUTTON_UP) {
		#endif
		#else
		if (PAD_ButtonsDown(0) & PAD_BUTTON_UP) {
		#endif
			render = true;
			rom_selected--;
			if (rom_selected < 0 || rom_selected >= roms.size())
				rom_selected = roms.size() - 1;
		}

		#ifdef HW_RVL
		#ifdef USE_WIIDRC
			if ((hasDRC && drcdat->button & WIIDRC_BUTTON_DOWN) || WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN || PAD_ButtonsDown(0) & PAD_BUTTON_DOWN) {
		#else
			if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN || PAD_ButtonsDown(0) & PAD_BUTTON_DOWN) {
		#endif
		#else
		if (PAD_ButtonsDown(0) & PAD_BUTTON_DOWN) {
		#endif
			render = true;
			rom_selected++;
			if (rom_selected >= roms.size())
				rom_selected = 0;
		}

		#ifdef HW_RVL
		#ifdef USE_WIIDRC
			if ((hasDRC && drcdat->button & WIIDRC_BUTTON_A) || WPAD_ButtonsDown(0) & WPAD_BUTTON_A || PAD_ButtonsDown(0) & PAD_BUTTON_A) {
		#else
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A || PAD_ButtonsDown(0) & PAD_BUTTON_A) {
		#endif
		#else
		if (PAD_ButtonsDown(0) & PAD_BUTTON_A) {
		#endif
			inMenu = false;
		}

		#ifdef HW_RVL
		#ifdef USE_WIIDRC
			if ((hasDRC && drcdat->button & WIIDRC_BUTTON_HOME) || WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME || PAD_ButtonsDown(0) & PAD_BUTTON_B || reset) {
		#else
			if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME || PAD_ButtonsDown(0) & PAD_BUTTON_B || reset) {
		#endif
		#else
		if (PAD_ButtonsDown(0) & PAD_BUTTON_B || reset) {
		#endif
			break;
		}

		if (render) {
			PrintHeader();
			#ifdef HW_RVL
				printf("Up/Down to navigate, A to select, HOME (Wiimote) or Start+Z (GC) to exit\n\n");
			#else
				printf("Up/Down to navigate, A to select, Start+Z to exit\n\n");
			#endif
			for (unsigned int i = 0; i < roms.size(); i++) {
				if (i == rom_selected)	{ printf("> "); }
				else					{ printf("  "); }

				printf("%s\n", roms[i].c_str());
			}
			render = false;
		}
	}

	if (inMenu) {
		exit(0);
	} else {
		PrintHeader();

		std::string cart_name = appPath + "/roms/" + roms[rom_selected] + ".bin";
		std::ifstream cart_file(cart_name, std::ios::binary);
		config.cart.rom.assign(std::istreambuf_iterator<char>(cart_file), {});
		cart_file.close();

		//Determine the size of SRAM from the cartridge header
		uint32_t sram_start, sram_end;
		memcpy(&sram_start, config.cart.rom.data() + 0x10, 4);
		memcpy(&sram_end, config.cart.rom.data() + 0x14, 4);
		uint32_t sram_size = Common::bswp32(sram_end) - Common::bswp32(sram_start) + 1;

		//Attempt to load SRAM from a file
		config.cart.sram_file_path = remove_extension(cart_name) + ".sav";
		std::ifstream sram_file(config.cart.sram_file_path, std::ios::binary);
		if (!sram_file.is_open())
		{
			printf("Warning: SRAM not found\n");
		}
		else
		{
			printf("Successfully found SRAM\n");
			config.cart.sram.assign(std::istreambuf_iterator<char>(sram_file), {});
			sram_file.close();
		}

		//Ensure SRAM is at the proper size. If no file is loaded, it will be filled with 0xFF.
		//If a file was loaded but was smaller than the SRAM size, the uninitialized bytes will be 0xFF.
		//If the file was larger, then the vector size is clamped
		config.cart.sram.resize(sram_size, 0xFF);

		//Initialize the emulator and all of its subprojects
		printf("Starting emulator\n");
		System::initialize(config);
		Sound::set_mute(false);

		//All subprojects have been initialized, so it is safe to reference them now
		if (!SDL::initialize())
			fatal("failed to init SDL");

		while (!shutdown)
		{
			PAD_ScanPads();
			if (PAD_ButtonsHeld(0) & (PAD_BUTTON_START | PAD_TRIGGER_Z)) {
				exit(0);
			}

			#ifdef HW_RVL
			WPAD_ScanPads();
			if (WPAD_ButtonsHeld(0) & WPAD_BUTTON_HOME) {
				exit(0);
			}
			#endif

			if (shutdown) {
				break;
			}

			if (reset) {
				printf("Rebooting Loopy...\n");
				System::shutdown(config);
				System::initialize(config);
				reset = false;
			}

			System::run();
			SDL::update(System::get_display_output());

			#ifdef HW_RVL
			WPADData* data_wpad = WPAD_Data(0);
			#ifdef USE_WIIDRC
			WiiDRC_ScanPads();
			const struct WiiDRCData *drcdat = WiiDRC_Data();
			if (hasDRC) {
				// LoopyIO::update_pad(Input::PAD_PRESENCE,	drcdat->button & WIIDRC_BUTTON_MINUS);
				LoopyIO::update_pad(Input::PAD_START,		drcdat->button & WIIDRC_BUTTON_PLUS);
				LoopyIO::update_pad(Input::PAD_L1,			drcdat->button & WIIDRC_BUTTON_L);
				LoopyIO::update_pad(Input::PAD_R1,			drcdat->button & WIIDRC_BUTTON_R);
				LoopyIO::update_pad(Input::PAD_A,			drcdat->button & WIIDRC_BUTTON_A);
				LoopyIO::update_pad(Input::PAD_B,			drcdat->button & WIIDRC_BUTTON_B);
				LoopyIO::update_pad(Input::PAD_C,			drcdat->button & WIIDRC_BUTTON_X);
				LoopyIO::update_pad(Input::PAD_D,			drcdat->button & WIIDRC_BUTTON_Y);
				LoopyIO::update_pad(Input::PAD_UP,			drcdat->button & WIIDRC_BUTTON_UP);
				LoopyIO::update_pad(Input::PAD_DOWN,		drcdat->button & WIIDRC_BUTTON_DOWN);
				LoopyIO::update_pad(Input::PAD_LEFT,		drcdat->button & WIIDRC_BUTTON_LEFT);
				LoopyIO::update_pad(Input::PAD_RIGHT,		drcdat->button & WIIDRC_BUTTON_RIGHT);
			} else if (data_wpad->exp.type != WPAD_EXP_CLASSIC) {
			#else
			if (data_wpad->exp.type != WPAD_EXP_CLASSIC) {
			#endif
				// LoopyIO::update_pad(Input::PAD_PRESENCE,	WPAD_ButtonsHeld(0) & WPAD_BUTTON_MINUS				|| PAD_ButtonsHeld(0) & PAD_TRIGGER_Z);
				LoopyIO::update_pad(Input::PAD_START,		WPAD_ButtonsHeld(0) & WPAD_BUTTON_PLUS				|| PAD_ButtonsHeld(0) & PAD_BUTTON_START);
				LoopyIO::update_pad(Input::PAD_L1,			WPAD_ButtonsHeld(0) & WPAD_NUNCHUK_BUTTON_Z			|| PAD_ButtonsHeld(0) & PAD_TRIGGER_L);
				LoopyIO::update_pad(Input::PAD_R1,			WPAD_ButtonsHeld(0) & WPAD_NUNCHUK_BUTTON_C			|| PAD_ButtonsHeld(0) & PAD_TRIGGER_R);
				LoopyIO::update_pad(Input::PAD_A,			WPAD_ButtonsHeld(0) & WPAD_BUTTON_2					|| PAD_ButtonsHeld(0) & PAD_BUTTON_A);
				LoopyIO::update_pad(Input::PAD_B,			WPAD_ButtonsHeld(0) & WPAD_BUTTON_1					|| PAD_ButtonsHeld(0) & PAD_BUTTON_B);
				LoopyIO::update_pad(Input::PAD_C,			WPAD_ButtonsHeld(0) & WPAD_BUTTON_A					|| PAD_ButtonsHeld(0) & PAD_BUTTON_X);
				LoopyIO::update_pad(Input::PAD_D,			WPAD_ButtonsHeld(0) & WPAD_BUTTON_B					|| PAD_ButtonsHeld(0) & PAD_BUTTON_Y);
				LoopyIO::update_pad(Input::PAD_UP,			WPAD_ButtonsHeld(0) & WPAD_BUTTON_RIGHT				|| PAD_ButtonsHeld(0) & PAD_BUTTON_UP);
				LoopyIO::update_pad(Input::PAD_DOWN,		WPAD_ButtonsHeld(0) & WPAD_BUTTON_LEFT				|| PAD_ButtonsHeld(0) & PAD_BUTTON_DOWN);
				LoopyIO::update_pad(Input::PAD_LEFT,		WPAD_ButtonsHeld(0) & WPAD_BUTTON_UP				|| PAD_ButtonsHeld(0) & PAD_BUTTON_LEFT);
				LoopyIO::update_pad(Input::PAD_RIGHT,		WPAD_ButtonsHeld(0) & WPAD_BUTTON_DOWN				|| PAD_ButtonsHeld(0) & PAD_BUTTON_RIGHT);
			} else {
				// LoopyIO::update_pad(Input::PAD_PRESENCE,	WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_MINUS		|| PAD_ButtonsHeld(0) & PAD_TRIGGER_Z);
				LoopyIO::update_pad(Input::PAD_START,		WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_PLUS		|| PAD_ButtonsHeld(0) & PAD_BUTTON_START);
				LoopyIO::update_pad(Input::PAD_L1,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_FULL_L	|| PAD_ButtonsHeld(0) & PAD_TRIGGER_L);
				LoopyIO::update_pad(Input::PAD_R1,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_FULL_R	|| PAD_ButtonsHeld(0) & PAD_TRIGGER_R);
				LoopyIO::update_pad(Input::PAD_A,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_A			|| PAD_ButtonsHeld(0) & PAD_BUTTON_A);
				LoopyIO::update_pad(Input::PAD_B,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_B			|| PAD_ButtonsHeld(0) & PAD_BUTTON_B);
				LoopyIO::update_pad(Input::PAD_C,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_X			|| PAD_ButtonsHeld(0) & PAD_BUTTON_X);
				LoopyIO::update_pad(Input::PAD_D,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_Y			|| PAD_ButtonsHeld(0) & PAD_BUTTON_Y);
				LoopyIO::update_pad(Input::PAD_UP,			WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_UP		|| PAD_ButtonsHeld(0) & PAD_BUTTON_UP);
				LoopyIO::update_pad(Input::PAD_DOWN,		WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_DOWN		|| PAD_ButtonsHeld(0) & PAD_BUTTON_DOWN);
				LoopyIO::update_pad(Input::PAD_LEFT,		WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_LEFT		|| PAD_ButtonsHeld(0) & PAD_BUTTON_LEFT);
				LoopyIO::update_pad(Input::PAD_RIGHT,		WPAD_ButtonsHeld(0) & WPAD_CLASSIC_BUTTON_RIGHT		|| PAD_ButtonsHeld(0) & PAD_BUTTON_RIGHT);
			}
			#else
				// LoopyIO::update_pad(Input::PAD_PRESENCE,	PAD_ButtonsHeld(0) & PAD_TRIGGER_Z);
				LoopyIO::update_pad(Input::PAD_START,		PAD_ButtonsHeld(0) & PAD_BUTTON_START);
				LoopyIO::update_pad(Input::PAD_L1,			PAD_ButtonsHeld(0) & PAD_TRIGGER_L);
				LoopyIO::update_pad(Input::PAD_R1,			PAD_ButtonsHeld(0) & PAD_TRIGGER_R);
				LoopyIO::update_pad(Input::PAD_A,			PAD_ButtonsHeld(0) & PAD_BUTTON_A);
				LoopyIO::update_pad(Input::PAD_B,			PAD_ButtonsHeld(0) & PAD_BUTTON_B);
				LoopyIO::update_pad(Input::PAD_C,			PAD_ButtonsHeld(0) & PAD_BUTTON_X);
				LoopyIO::update_pad(Input::PAD_D,			PAD_ButtonsHeld(0) & PAD_BUTTON_Y);
				LoopyIO::update_pad(Input::PAD_UP,			PAD_ButtonsHeld(0) & PAD_BUTTON_UP);
				LoopyIO::update_pad(Input::PAD_DOWN,		PAD_ButtonsHeld(0) & PAD_BUTTON_DOWN);
				LoopyIO::update_pad(Input::PAD_LEFT,		PAD_ButtonsHeld(0) & PAD_BUTTON_LEFT);
				LoopyIO::update_pad(Input::PAD_RIGHT,		PAD_ButtonsHeld(0) & PAD_BUTTON_RIGHT);
			#endif
		}

		System::shutdown(config);
		SDL::shutdown();

		if (shutdown) {
			SYS_ResetSystem(SYS_POWEROFF, 0, 0);
		}
	}

	return 0;
}