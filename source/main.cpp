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
#include <fstream>

#include <SDL2/SDL.h>

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

	constexpr static int FRAME_WIDTH = 640;
	constexpr static int FRAME_HEIGHT = 480;

	// Scales the frame size up to 4:3 640x480
	static constexpr float ASPECT_CORRECT_SCALE_X = (640.0f / FRAME_WIDTH);

	struct Screen
	{
		SDL_Renderer* renderer;
		SDL_Window* window;
		SDL_Texture* texture;

		SDL_Texture* prescaled;
		int visible_scanlines = DISPLAY_HEIGHT;
		int window_int_scale = 1;
		int prescale = 1;
		bool correct_aspect_ratio;
		bool crop_overscan;
		bool antialias;
	};

	static Screen screen;

	bool initialize()
	{
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			printf("SDL2 error: %s\n", SDL_GetError());
			return false;
		}

		//Try synchronizing drawing to VBLANK
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

		//Set up SDL screen parameters
		screen.correct_aspect_ratio = true;
		screen.crop_overscan = false;
		screen.antialias = false;
		screen.window_int_scale = 1;
		screen.prescale = screen.antialias ? 4 : 1;

		//Set up SDL screen
		SDL_CreateWindowAndRenderer(2 * FRAME_WIDTH, 2 * FRAME_HEIGHT, 0, &screen.window, &screen.renderer);
		SDL_SetWindowTitle(screen.window, "Rupi");
		SDL_SetWindowSize(screen.window, 2 * FRAME_WIDTH, 2 * FRAME_HEIGHT);
		SDL_SetWindowResizable(screen.window, SDL_FALSE);
		SDL_RenderSetLogicalSize(screen.renderer, 2 * FRAME_WIDTH, 2 * FRAME_HEIGHT);

		screen.texture = SDL_CreateTexture(screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);

		SDL_SetTextureBlendMode(screen.texture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(screen.texture, SDL_ScaleModeNearest);

		if (screen.prescale > 1)
		{
			screen.prescaled = SDL_CreateTexture(
				screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_TARGET, DISPLAY_WIDTH * screen.prescale,
				DISPLAY_HEIGHT * screen.prescale
			);
			SDL_SetTextureBlendMode(screen.prescaled, SDL_BLENDMODE_BLEND);
			SDL_SetTextureScaleMode(screen.prescaled, SDL_ScaleModeBest);
		}
		return true;
	}

	void shutdown() {
		//Destroy window, then kill SDL2
		SDL_DestroyTexture(screen.texture);
		SDL_DestroyRenderer(screen.renderer);
		SDL_DestroyWindow(screen.window);

		SDL_Quit();
	}

	void update(uint16_t* display_output)
	{
		// Draw screen
		void* pixels;
		int pitch;
		// More efficient alternative to SDL_UpdateTexture(screen.texture, NULL, display_output, sizeof(uint16_t) * DISPLAY_WIDTH);
		if (SDL_LockTexture(screen.texture, nullptr, &pixels, &pitch) == 0)
		{
			memcpy(pixels, display_output, sizeof(uint16_t) * DISPLAY_WIDTH * DISPLAY_HEIGHT);
			SDL_UnlockTexture(screen.texture);
		}
		// SDL_RenderCopy(screen.renderer, screen.texture, NULL, NULL);
		// SDL_RenderPresent(screen.renderer);

		// Prescale
		if (screen.prescaled)
		{
			SDL_SetRenderTarget(screen.renderer, screen.prescaled);
			SDL_RenderClear(screen.renderer);
			SDL_RenderCopy(screen.renderer, screen.texture, nullptr, nullptr);
		}

		// Change target back to screen (must be done before querying renderer output size!)
		SDL_SetRenderTarget(screen.renderer, nullptr);
		SDL_RenderClear(screen.renderer);

		SDL_Rect src = {0, 0, DISPLAY_WIDTH * screen.prescale, DISPLAY_HEIGHT * screen.prescale};
		SDL_Rect frame = {0, 0, FRAME_WIDTH * screen.prescale, FRAME_HEIGHT * screen.prescale};
		if (screen.crop_overscan) frame = src;
		SDL_Rect dest = {0};
		SDL_GetRendererOutputSize(screen.renderer, &dest.w, &dest.h);

		float scale_x = (float)dest.w / frame.w;
		float scale_y = (float)dest.h / frame.h;
		float scale = SDL_min(scale_x, scale_y);
		if (!screen.antialias && !screen.correct_aspect_ratio)
		{
			scale = SDL_floorf(scale);
		}
		scale_x = scale_y = scale;
		if (screen.correct_aspect_ratio)
		{
			scale_x *= ASPECT_CORRECT_SCALE_X;
		}
		float w = scale_x * src.w;
		float h = scale_y * src.h;
		dest.x = (dest.w - w) / 2;
		dest.y = (dest.h - h) / 2;
		dest.w = w;
		dest.h = h;

		SDL_RenderCopy(screen.renderer, (screen.prescaled ? screen.prescaled : screen.texture), &src, &dest);
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

	// Initialise the console
	/*GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
	void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();*/

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

	#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);
	#endif

	std::string cart_name = appPath + "/rom.bin";
	std::string bios_name = appPath + "/bios.bin";
	std::string sound_rom_name = appPath + "/soundbios.bin";

	Config::SystemInfo config;

	std::ifstream cart_file(cart_name, std::ios::binary);
	if (!cart_file.is_open())
	{
		fatal("rom.bin not found");
	}
	config.cart.rom.assign(std::istreambuf_iterator<char>(cart_file), {});
	cart_file.close();

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
	}
	config.sound_rom.assign(std::istreambuf_iterator<char>(sound_rom_file), {});
	sound_rom_file.close();

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
	// printf("Resizing SRAM\n");
	// config.cart.sram.resize(sram_size, 0xFF);

	//Initialize the emulator and all of its subprojects
	printf("Starting virtual machine\n");
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

	printf("Switching to SDL\n");
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

		/*SDL_Event e;
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
		}*/

	}

	// System::shutdown();
	VIDEO_SetBlack(true);
	// SDL::shutdown();

	// fatUnmount(0);

	return 0;
}