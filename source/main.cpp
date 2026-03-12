#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <climits>

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>

#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include "core/system.h"
#include "core/config.h"
#include "common/bswp.h"
#include "input/input.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static std::string devicePrefix;

static std::string remove_extension(std::string file_path)
{
	auto pos = file_path.find(".");
	if (pos == std::string::npos)
	{
		return file_path;
	}

	return file_path.substr(0, pos);
}

// Global Screen Resources
static constexpr int DISPLAY_WIDTH = 640;
static constexpr int DISPLAY_HEIGHT = 480;
static constexpr int PRESCALE_FACTOR = 4;
// Logical size includes border to allow for both 224 line and 240 line modes, and show some of the background effects.
// In reality the Loopy active area is drawn inside this and borders are visible, in 240 line mode more of this is used.
static constexpr int FRAME_WIDTH = 280;
static constexpr int FRAME_HEIGHT = 240;
// Scales the frame size up to 4:3 320x240
static constexpr float ASPECT_CORRECT_SCALE_X = (320.0f / FRAME_WIDTH);

struct Screen
{
	SDL_Renderer* renderer;
	SDL_Window* window;
	SDL_Texture* framebuffer;
};

static Screen screen;

static void update(uint16_t* display_output)
{
	// Change target back to screen (must be done before querying renderer output size!)
	SDL_SetRenderTarget(screen.renderer, nullptr);
	SDL_SetRenderDrawColor(screen.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(screen.renderer);

    SDL_UpdateTexture(screen.framebuffer, NULL, display_output, sizeof(uint16_t) * DISPLAY_WIDTH);
    SDL_RenderCopy(screen.renderer, screen.framebuffer, NULL, NULL);
    SDL_RenderPresent(screen.renderer);
}

// SDL2 Renderer Setup
static bool initSdl2()
{
	//Allow use of our own main()
	SDL_SetMainReady();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
	{
		printf("SDL2: %s\n", SDL_GetError());
		return false;
	}

    // make sure SDL cleans up before exit
    // atexit(SDL_Quit);
    SDL_ShowCursor(SDL_DISABLE);

    //Try synchronizing drawing to VBLANK
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    //Set up SDL screen
    SDL_CreateWindowAndRenderer(2 * DISPLAY_WIDTH, 2 * DISPLAY_HEIGHT, 0, &screen.window, &screen.renderer);
    SDL_SetWindowTitle(screen.window, "Rupi");
    SDL_SetWindowSize(screen.window, 2 * DISPLAY_WIDTH, 2 * DISPLAY_HEIGHT);
    SDL_SetWindowResizable(screen.window, SDL_FALSE);
    SDL_RenderSetLogicalSize(screen.renderer, 2 * DISPLAY_WIDTH, 2 * DISPLAY_HEIGHT);

    screen.framebuffer = SDL_CreateTexture(screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);

	// Mouse mappings don't really need configuration
	Input::add_mouse_binding(SDL_BUTTON_LEFT, Input::MouseButton::MOUSE_L);
	Input::add_mouse_binding(SDL_BUTTON_RIGHT, Input::MouseButton::MOUSE_R);
	return true;
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

static void cbShutdown() { shutdown = true; SYS_ResetSystem(SYS_POWEROFF, 0, 0); exit(0); }
static void cbShutdownWpad(s32 chan) { shutdown = true; SYS_ResetSystem(SYS_POWEROFF, 0, 0); exit(0); }
static void cbReset(u32 chan, void* arg) { exit(0); }

static void fatal(const char *txt)
{
	printf("--ERROR--\n%s. Exiting in 5 seconds\n", txt);
	perror("msg");
	sleep(5);
	exit(0);
}

static std::string appPath;
static void CreateAppPath(char * origpath)
{
	fs::path p = origpath;
	appPath = p.parent_path();

	// FOR DOLPHIN ONLY
	if (strcmp(origpath, "app") == 0) { appPath = "sd:/apps/LoopyMSE-Wii"; }

	printf("Running from %s\n", appPath.c_str());
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

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	SYS_SetPowerCallback(cbShutdown);
	WPAD_SetPowerButtonCallback(cbShutdownWpad);
	SYS_SetResetCallback(cbReset);

	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );
	printf("\x1b[2;0H");

	printf("LoopyMSE-Wii v0.1\n\n");

	if (!initFat()) {
		fatal("failed to init libfat");
	}

	Config::SystemInfo config;
	
	#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);
	#endif

	std::string cart_name = appPath + "/rom.bin";
	std::string bios_name = appPath + "/bios.bin";
	std::string sound_rom_name = appPath + "/soundbios.bin";

	std::ifstream cart_file(cart_name, std::ios::binary);
	if (!cart_file.is_open()) { fatal("failed to open ROM"); }
	config.cart.rom.assign(std::istreambuf_iterator<char>(cart_file), {});
	cart_file.close();

	std::ifstream bios_file(bios_name, std::ios::binary);
	if (!bios_file.is_open()) { fatal("failed to open BIOS"); }
	config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
	bios_file.close();

	// std::ifstream sound_rom_file(sound_rom_name, std::ios::binary);
	// if (!sound_rom_file.is_open())
		// printf("Warning: Sound ROM not detected\n");
	// else {
		// config.sound_rom.assign(std::istreambuf_iterator<char>(sound_rom_file), {});
		// sound_rom_file.close();
	// }

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

		//Ensure SRAM is at the proper size. If no file is loaded, it will be filled with 0xFF.
		//If a file was loaded but was smaller than the SRAM size, the uninitialized bytes will be 0xFF.
		//If the file was larger, then the vector size is clamped
		// config.cart.sram.resize(sram_size, 0xFF);
	}

	//Initialize the emulator and all of its subprojects
	printf("Starting...\n");

	if (!initSdl2()) {
		fatal("failed to init SDL2");
	}

	System::initialize(config);

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

	bool has_quit = false;
	uint64_t last_frame_ticks = SDL_GetPerformanceCounter();
	while (!has_quit)
	{
		constexpr int framerate_target = 60;  //TODO: get this from Video if it can be changed (e.g. for PAL mode)
		constexpr int framerate_max_lag = 5;
		//Check how much time passed since we drew the last frame
		uint64_t ticks_per_frame = SDL_GetPerformanceFrequency() / framerate_target;
		uint64_t now_ticks = SDL_GetPerformanceCounter();
		uint64_t ticks_since_last_frame = now_ticks - last_frame_ticks;

		//See how many we need to draw
		//If we're vsynced to a 60Hz display with no lag, this should stay at 1 most of the time
		uint64_t draw_frames = ticks_since_last_frame / ticks_per_frame;
		last_frame_ticks += draw_frames * ticks_per_frame;

		//If too far behind, draw one frame and start timing again from now
		if (draw_frames > framerate_max_lag)
		{
			// printf("%d frames behind, skipping ahead...", draw_frames);
			last_frame_ticks = now_ticks;
			draw_frames = 1;
		}

		if (draw_frames && config.cart.is_loaded())
		{
			while (draw_frames > 0)
			{
				System::run();
				draw_frames--;
			}

			// Draw screen
			update(System::get_display_output());
		}

		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_QUIT:
				has_quit = true;
				break;
			case SDL_JOYDEVICEADDED:
				SDL_JoystickOpen(0);
				break;
			case SDL_JOYBUTTONDOWN:
				Input::set_key_state(e.key.keysym.sym, true);
				break;
			case SDL_JOYBUTTONUP:
				Input::set_key_state(e.key.keysym.sym, false);
				break;
			}
		}

        WPAD_ScanPads();
        if ((WPAD_ButtonsHeld(0) & WPAD_BUTTON_HOME) || shutdown)
			has_quit = true;
	}

	printf("Stopped emulation, exiting\n");
	System::shutdown(config);

	//Destroy window, then kill SDL2
	SDL_DestroyTexture(screen.framebuffer);
	SDL_DestroyRenderer(screen.renderer);
	SDL_DestroyWindow(screen.window);
	VIDEO_SetBlack(true);
	SDL_Quit();

	// fatUnmount(0);

	return 0;
}