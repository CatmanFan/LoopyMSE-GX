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
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <SDL2/SDL.h>
#include <SDL2/sdl_image.h>

#include "common/bswp.h"
#include "core/config.h"
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
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
		{
			printf("SDL2 error: %s\n", SDL_GetError());
			return false;
		}

		//Try synchronizing drawing to VBLANK
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
 
		// make sure SDL cleans up before exit
		atexit(SDL_Quit);
		SDL_ShowCursor(SDL_DISABLE);

		// create a new window
		screen.window = SDL_CreateWindow(
			"Rupi",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			640, 480,
			SDL_WINDOW_SHOWN
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

static bool shutdown = false;

static void cbShutdown() { shutdown = true; }
static void cbShutdownWpad(s32 chan) { shutdown = true; }
static void cbReset(u32 chan, void* arg) { exit(0); }

static void fatal(const char *txt)
{
	printf("HALT: %s, exiting in 5 seconds\n", txt);
	// perror("msg");
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

	printf("Running from %s\n", appPath.c_str());
}

static void PrintHeader()
{
    printf("\033[2J\033[H"); // Clear screen
	printf("LoopyMSE-Wii v0.1\n");
	printf("-----------------------------------------------------------------------------\n");
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	PAD_Init();
	WPAD_Init();
	// Enable all buttons and accelerometer for all connected controllers
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);

	SYS_SetPowerCallback(cbShutdown);
	WPAD_SetPowerButtonCallback(cbShutdownWpad);
	SYS_SetResetCallback(cbReset);

	// Initialise the console
	GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
	void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	PrintHeader();
	printf("Loading\n");

	if (!initFat()) {
		fatal("failed to init libfat");
	}

	#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);
	#endif

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
		printf("Warning: soundbios.bin not found");
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
		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_UP) {
			render = true;
			rom_selected--;
			if (rom_selected < 0 || rom_selected >= roms.size())
				rom_selected = roms.size() - 1;
		}
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_DOWN) {
			render = true;
			rom_selected++;
			if (rom_selected >= roms.size())
				rom_selected = 0;
		}
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A) {
			inMenu = false;
		}
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) {
			break;
		}

		if (render) {
			PrintHeader();
			printf("Up/Down to select a ROM, A to launch, HOME to exit\n\n");
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
		Input::add_key_binding(SDLK_RETURN, Input::PAD_START);

		Input::add_key_binding(SDLK_z, Input::PAD_A);
		Input::add_key_binding(SDLK_x, Input::PAD_B);
		Input::add_key_binding(SDLK_a, Input::PAD_C);
		Input::add_key_binding(SDLK_s, Input::PAD_D);

		Input::add_key_binding(SDLK_q, Input::PAD_L1);
		Input::add_key_binding(SDLK_w, Input::PAD_R1);

		Input::add_key_binding(SDLK_LEFT, Input::PAD_LEFT);
		Input::add_key_binding(SDLK_RIGHT, Input::PAD_RIGHT);
		Input::add_key_binding(SDLK_UP, Input::PAD_UP);
		Input::add_key_binding(SDLK_DOWN, Input::PAD_DOWN);

		if (!SDL::initialize())
			fatal("failed to init SDL");

		while (SYS_MainLoop())
		{
			WPAD_ScanPads();
			if (WPAD_ButtonsHeld(0) & WPAD_BUTTON_HOME) {
				exit(0);
			}
			if (shutdown) {
				break;
			}

			System::run();
			SDL::update(System::get_display_output());

			/* Warning: SRAM not found
			[Sound] Using audio device (null)
			[Sound] Init uPD937 core: synth rate 84864.0, out rate 48000.0, buffer size 2048
			[Sound] Init filters
			[Sound] Schedule timeref 100 Hz
			[Video] write MODE: 0012
			[Video] write DISPMODE: 0000
			[Video] write BM_CTRL: 0001
			[Video] write COLORPRIO: 0040
			[Video] write LAYER_CTRL: AA46
			[Video] write OBJ_CTRL: 0100
			[Video] write BG_CTRL: 000F
			[Video] write BM0_SCREENX: 0000
			[Video] write BM0_SCREENY: 0000
			[Video] write BM0_SCROLLY: 0000
			[Video] write BM0_CLIPWIDTH: 00FF
			[Video] write BM0_HEIGHT: 01FF
			[Video] VSYNC start
			[Sound] Unmuted output
			[Video] VSYNC end
			[Video] VSYNC start
			[Video] VSYNC end
			[Video] VSYNC start */

			SDL_Event e;
			while (SDL_PollEvent(&e))
			{
				switch (e.type)
				{
				// case SDL_QUIT:
					// has_quit = true;
					// break;
				case SDL_KEYDOWN:
					Input::set_key_state(e.key.keysym.sym, true);
					break;
				case SDL_KEYUP:
					Input::set_key_state(e.key.keysym.sym, false);
					break;
				}
			}
		}

		System::shutdown();
		SDL::shutdown();
	}

	return 0;
}