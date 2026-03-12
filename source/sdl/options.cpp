#include "options.h"

#include <SDL2/SDL.h>

#include <fstream>
#include <iostream>

#include "imgwriter.h"
#include "input/input.h"
// #include "log/log.h"

// namespace po = boost::program_options;
namespace imagew = SDL::ImageWriter;

namespace Options
{

// static po::options_description commandline_opts = po::options_description("Usage");

void input_add_default_key_bindings()
{
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
}

void input_add_default_controller_bindings()
{
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_A, Input::PAD_A);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_B, Input::PAD_B);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_Y, Input::PAD_C);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_X, Input::PAD_D);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Input::PAD_L1);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Input::PAD_R1);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_LEFT, Input::PAD_LEFT);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Input::PAD_RIGHT);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_UP, Input::PAD_UP);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_DOWN, Input::PAD_DOWN);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_START, Input::PAD_START);
}

const std::unordered_map<std::string, Input::PadButton> KEYBOARD_CONFIG_KEY_TO_PAD_ENUM = {
	{"keyboard-map.pad_start", Input::PAD_START}, {"keyboard-map.pad_l1", Input::PAD_L1},
	{"keyboard-map.pad_r1", Input::PAD_R1},		  {"keyboard-map.pad_a", Input::PAD_A},
	{"keyboard-map.pad_d", Input::PAD_D},		  {"keyboard-map.pad_c", Input::PAD_C},
	{"keyboard-map.pad_b", Input::PAD_B},		  {"keyboard-map.pad_up", Input::PAD_UP},
	{"keyboard-map.pad_down", Input::PAD_DOWN},	  {"keyboard-map.pad_left", Input::PAD_LEFT},
	{"keyboard-map.pad_right", Input::PAD_RIGHT},
};
const std::unordered_map<std::string, Input::PadButton> CONTROLLER_CONFIG_KEY_TO_PAD_ENUM = {
	{"controller-map.pad_start", Input::PAD_START}, {"controller-map.pad_l1", Input::PAD_L1},
	{"controller-map.pad_r1", Input::PAD_R1},		{"controller-map.pad_a", Input::PAD_A},
	{"controller-map.pad_d", Input::PAD_D},			{"controller-map.pad_c", Input::PAD_C},
	{"controller-map.pad_b", Input::PAD_B},			{"controller-map.pad_up", Input::PAD_UP},
	{"controller-map.pad_down", Input::PAD_DOWN},	{"controller-map.pad_left", Input::PAD_LEFT},
	{"controller-map.pad_right", Input::PAD_RIGHT},
};

bool parse_config(fs::path config_path, Args& args)
{
	input_add_default_key_bindings();
	input_add_default_controller_bindings();
	return true;

	// Probably not the correct place for this, but should always run, until configuration exists
	/* input_add_default_controller_bindings();

	args.bios = vm["emulator.bios"].as<std::string>();
	args.sound_bios = vm["emulator.sound_bios"].as<std::string>();
	args.run_in_background = vm["emulator.run_in_background"].as<bool>();
	args.start_in_fullscreen = vm["emulator.start_in_fullscreen"].as<bool>();
	args.correct_aspect_ratio = vm["emulator.correct_aspect_ratio"].as<bool>();
	args.antialias = vm["emulator.antialias"].as<bool>();
	args.crop_overscan = vm["emulator.crop_overscan"].as<bool>();
	args.int_scale = vm["emulator.int_scale"].as<int>();

	args.screenshot_image_type = imagew::parse_image_type(
		vm["emulator.screenshot_image_type"].as<std::string>(), imagew::IMAGE_TYPE_DEFAULT
	);
	args.printer_correct_aspect_ratio = vm["printer.correct_aspect_ratio"].as<bool>();
	args.printer_image_type =
		imagew::parse_image_type(vm["printer.image_type"].as<std::string>(), imagew::IMAGE_TYPE_DEFAULT);
	args.printer_view_command = vm["printer.view_command"].as<std::string>();

	// Keymap
	for (const auto& [cfg_key, pad_key] : KEYBOARD_CONFIG_KEY_TO_PAD_ENUM)
	{
		if (!vm.count(cfg_key))
		{
			// Log::warn("Loopy %s <- [No key set]", cfg_key.c_str());
			continue;
		}
		std::string key_string = vm[cfg_key].as<std::string>();
		SDL_Keycode keycode = SDL_GetKeyFromName(key_string.c_str());

		if (keycode == SDLK_UNKNOWN)
		{
			// Log::error("Could not parse key '%s' defined by %s", key_string.c_str(), cfg_key.c_str());
		}
		else
		{
			Input::add_key_binding(keycode, pad_key);
		}
	}

	// Controller map
	for (const auto& [cfg_key, pad_key] : CONTROLLER_CONFIG_KEY_TO_PAD_ENUM)
	{
		if (!vm.count(cfg_key))
		{
			// Log::warn("Loopy %s <- [No controller button set]", cfg_key.c_str());
			continue;
		}
		std::string button_string = vm[cfg_key].as<std::string>();
		SDL_GameControllerButton button = SDL_GameControllerGetButtonFromString(button_string.c_str());

		// TODO handle axes for analog stick input

		if ((int)button == (int)SDL_CONTROLLER_AXIS_INVALID)
		{
			// Log::error("Could not parse game controller button '%s' defined by %s", button_string.c_str(), cfg_key.c_str());
		}
		else
		{
			Input::add_controller_binding(button, pad_key);
		}
	}

	return true; */
}

}  // namespace Options
