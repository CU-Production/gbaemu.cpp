
#include <cstdio>
#include <list>

#define SOKOL_IMPL
#define SOKOL_NO_ENTRY
#define SOKOL_GLCORE33
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "sokol_audio.h"
#include "sokol_time.h"
#include "imgui.h"
#include "imgui_memory_editor.h"
#define SOKOL_IMGUI_IMPL
#include "util/sokol_imgui.h"
#include <nfd.hpp>

#include "gba.hpp"
#include "arm7tdmidisasm.hpp"
#include "types.hpp"

// Argument Variables
bool argRomGiven;
std::filesystem::path argRomFilePath;
bool argBiosGiven;
std::filesystem::path argBiosFilePath;

constexpr auto cexprHash(const char *str, std::size_t v = 0) noexcept -> std::size_t {
	return (*str == 0) ? v : 31 * cexprHash(str + 1) + *str;
}

// Create emulator thread
GameBoyAdvance GBA;
std::thread emuThread(&GBACPU::run, std::ref(GBA.cpu));

// For fps counter
int renderThreadFps = 0;
int emuThreadFps = 0;
u64 lastFpsPoll = 0;

// Sokol graphics
constexpr auto GBA_WIDTH  = 240;
constexpr auto GBA_HEIGHT = 160;

sg_pass_action pass_action{};
sg_image lcd_texture;
uint32_t pixel_buffer[GBA_WIDTH * GBA_HEIGHT]; // For RGB565 -> RGB888 convert

// ImGui Windows
void mainMenuBar();
bool showDemoWindow;
bool showRomInfo;
void romInfoWindow();
bool showNoBios;
void noBiosWindow();
bool showCpuDebug;
void cpuDebugWindow();
bool showSystemLog;
void systemLogWindow();
bool showMemEditor;
void memEditorWindow();
#if BUILD_WITH_PPUDEBUG
#include "ppudebug.hpp"
#endif

void romFileDialog();
void biosFileDialog();
bool memEditorUnrestrictedWrites = false;
MemoryEditor memEditor;
ImU8 memEditorRead(const ImU8* data, size_t off);
void memEditorWrite(ImU8* data, size_t off, ImU8 d);
bool memEditorHighlight(const ImU8* data, size_t off);

// Input
sapp_keycode keymap[10] = {
    SAPP_KEYCODE_X, // Button A
    SAPP_KEYCODE_Z, // Button B
    SAPP_KEYCODE_BACKSPACE, // Select
    SAPP_KEYCODE_ENTER, // Start
    SAPP_KEYCODE_RIGHT, // Right
    SAPP_KEYCODE_LEFT, // Left
    SAPP_KEYCODE_UP, // Up
    SAPP_KEYCODE_DOWN, // Down
    SAPP_KEYCODE_S, // Button R
    SAPP_KEYCODE_A, // Button L
};
u16 lastJoypad;

// Audio stuff
void audioCallback(float* buffer, int num_frames, int num_channels);

// Everything else
std::atomic<bool> quit = false;
void loadRom();

// RGB555A1 to RGBA8888
u32 RGB5A1toRGBA8(u16 inRGB5A1);

// sokol callbacks
void init() {
    // Setup sokol
    sg_desc desc = {};
    desc.context = sapp_sgcontext();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    // Setup sokol-audio
    saudio_desc as_desc = {};
    as_desc.logger.func = slog_func;
    as_desc.sample_rate = 32768;
    as_desc.num_channels = 2;
    as_desc.stream_cb = audioCallback;
    as_desc.buffer_frames = 1024;
    saudio_setup(&as_desc);

    // setup sokol-imgui
    simgui_desc_t simgui_desc = {};
    simgui_setup(&simgui_desc);

    // setup sokol_time
    stm_setup();
    lastFpsPoll = stm_now();

    sg_color clear_color = sg_color(0.45f, 0.55f, 0.60f, 1.00f);
    pass_action.colors[0] = { .action=SG_ACTION_CLEAR, .value=clear_color };

    // Init GBA emucore
    // Create image for main display
    sg_image_desc img_desc = {};
    img_desc.width = GBA_WIDTH;
    img_desc.height = GBA_HEIGHT;
    img_desc.num_mipmaps = 1;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.usage = SG_USAGE_STREAM;
    img_desc.min_filter = SG_FILTER_NEAREST;
    img_desc.mag_filter = SG_FILTER_NEAREST;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    lcd_texture = sg_make_image(&img_desc);

#if BUILD_WITH_PPUDEBUG
    initPpuDebug();
#endif
}

void frame() {
    // GBA emu main loop
    if (GBA.ppu.updateScreen) {
        for (int y = 0; y < GBA_HEIGHT; y++) {
            for (int x = 0; x < GBA_WIDTH; x++) {
                pixel_buffer[y * GBA_WIDTH + x] = RGB5A1toRGBA8(GBA.ppu.framebuffer[y][x]);
            }
        }

        sg_image_data image_data{};
        image_data.subimage[0][0] = { .ptr=pixel_buffer, .size=(GBA_WIDTH * GBA_HEIGHT * sizeof(uint32_t)) };
        sg_update_image(lcd_texture, image_data);
        GBA.ppu.updateScreen = false;
    }

    /* Draw ImGui Stuff */
    const int width = sapp_width();
    const int height = sapp_height();
    simgui_new_frame({ width, height, sapp_frame_duration(), sapp_dpi_scale() });
    ImGuiIO& io = ImGui::GetIO();

    mainMenuBar();

    if (showDemoWindow)
        ImGui::ShowDemoWindow(&showDemoWindow);
    if (showRomInfo)
        romInfoWindow();
    if (showNoBios)
        noBiosWindow();
    if (showCpuDebug)
        cpuDebugWindow();
    if (showSystemLog)
        systemLogWindow();
    if (showMemEditor)
        memEditorWindow();
#if BUILD_WITH_PPUDEBUG
    if (showLayerView)
        layerViewWindow();
    if (showTiles)
        tilesWindow();
    if (showPalette)
        paletteWindow();
#endif

    /* compute elapsed time */
    uint64_t elapsed = stm_since(lastFpsPoll);
    double milliseconds = stm_ms(elapsed);
    if (milliseconds >= 1000) {
        lastFpsPoll = stm_now();
        renderThreadFps = (int)io.Framerate;
        emuThreadFps = GBA.ppu.frameCounter;
        GBA.ppu.frameCounter = 0;
    }

    // Console Screen
    {
        ImGui::Begin("Game Boy Advance Screen");

        ImGui::Text("Rendering Thread:  %d FPS", renderThreadFps);
        ImGui::Text("Emulator Thread:   %d FPS", emuThreadFps);
        ImGui::Image((ImTextureID)(uintptr_t)lcd_texture.id, ImVec2(240 * 3, 160 * 3));

        ImGui::End();
    }

    /* Rendering */
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    simgui_render();
    sg_end_pass();
    sg_commit();
}

void cleanup() {
    GBA.cpu.addThreadEvent(GBACPU::STOP, (u64)0);
//    emuThread.detach();

    simgui_shutdown();
    saudio_shutdown();
    sg_shutdown();
}

u16 currentJoypad = 0;
void input(const sapp_event* event) {
    // Joypad inputs
    if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        for (int i = 0; i < 10; i++) {
            if (event->key_code == keymap[i])
                currentJoypad |= 1 << i;
        }
    } else if (event->type == SAPP_EVENTTYPE_KEY_UP) {
        for (int i = 0; i < 10; i++) {
            if (event->key_code == keymap[i])
                currentJoypad -= 1 << i;
        }
    }

    if (currentJoypad != lastJoypad) {
        GBA.cpu.addThreadEvent(GBACPU::UPDATE_KEYINPUT, ~currentJoypad);
        lastJoypad = currentJoypad;
    }

    // load rom/bios and save
    if (event->modifiers & SAPP_MODIFIER_CTRL && event->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (event->key_code == SAPP_KEYCODE_S) {
            GBA.save();
        }
        if (event->key_code == SAPP_KEYCODE_O) {
            if (event->modifiers &  SAPP_MODIFIER_SHIFT) {
                biosFileDialog();
            } else {
                romFileDialog();
            }
        }
    }

    simgui_handle_event(event);

}


int main(int argc, char *argv[]) {
	// Parse arguments
	argRomGiven = false;
	argBiosGiven = false;
	argBiosFilePath = "";
	for (int i = 1; i < argc; i++) {
		switch (cexprHash(argv[i])) {
		case cexprHash("--rom"):
			if (argc == (++i)) {
				printf("Not enough arguments for flag --rom\n");
				return -1;
			}
			argRomGiven = true;
			argRomFilePath = argv[i];
			break;
		case cexprHash("--bios"):
			if (argc == ++i) {
				printf("Not enough arguments for flag --bios\n");
				return -1;
			}
			argBiosGiven = true;
			argBiosFilePath = argv[i];
			break;
		default:
			if (i == 1) {
				argRomGiven = true;
				argRomFilePath = argv[i];
			} else {
				printf("Unknown argument:  %s\n", argv[i]);
				return -1;
			}
			break;
		}
	}
	if (argRomGiven)
		loadRom();
	disassembler.defaultSettings();

    memEditor.ReadFn = memEditorRead;
    memEditor.WriteFn = memEditorWrite;
    memEditor.HighlightFn = memEditorHighlight;

    NFD::Guard nfdGuard;

    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = input;
    desc.width = 1280;
    desc.height = 720;
    std::string windowName = "gbaemu.cpp - ";
    windowName += argRomFilePath.string();
    desc.window_title = windowName.c_str();
    desc.swap_interval = 0;
    desc.icon.sokol_default = true,
    desc.logger.func = slog_func;
    sapp_run(desc);

    emuThread.detach();

	return 0;
}


void audioCallback(float* buffer, int num_frames, int num_channels) {
	GBA.apu.sampleBufferMutex.lock();
    for (int i = 0; i < num_frames * num_channels; i++) {
        buffer[i] = GBA.apu.sampleBuffer[i] / float(0x7FFF);
    }
    // If there aren't enough samples, repeat the last one
    int realIndex = (GBA.apu.sampleBufferIndex - 2) % 2048;
    for (int i = GBA.apu.sampleBufferIndex; i < num_frames * num_channels / 2; i += 2) {
        buffer[i] = GBA.apu.sampleBuffer[realIndex] / float(0x7FFF);
        buffer[i + 1] = GBA.apu.sampleBuffer[realIndex + 1] / float(0x7FFF);
    }

	GBA.apu.sampleBufferIndex = 0;
	GBA.apu.apuBlock = false;
	GBA.apu.sampleBufferMutex.unlock();
}

void loadRom() {
	GBA.cpu.addThreadEvent(GBACPU::STOP);
	GBA.cpu.addThreadEvent(GBACPU::LOAD_BIOS, &argBiosFilePath);
	GBA.cpu.addThreadEvent(GBACPU::LOAD_ROM, &argRomFilePath);
	GBA.cpu.addThreadEvent(GBACPU::RESET);
	GBA.cpu.addThreadEvent(GBACPU::START);

	GBA.cpu.uncapFps = false;
}

void mainMenuBar() {
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Load ROM", "Ctrl+O")) {
			romFileDialog();
		}

		if (ImGui::MenuItem("Load Bios", "Ctrl+Shift+O")) {
			biosFileDialog();
		}

		if (ImGui::MenuItem("Save", "Ctrl+S", false, argRomGiven)) {
			GBA.save();
		}

		ImGui::Separator();
		ImGui::MenuItem("ROM Info", nullptr, &showRomInfo, argRomGiven);

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Emulation")) {
		if (GBA.cpu.running) {
			if (ImGui::MenuItem("Pause"))
				GBA.cpu.addThreadEvent(GBACPU::STOP, (u64)0);
		} else {
			if (ImGui::MenuItem("Unpause"))
				GBA.cpu.addThreadEvent(GBACPU::START);
		}
		if (ImGui::MenuItem("Reset")) {
			GBA.cpu.addThreadEvent(GBACPU::RESET);
			GBA.cpu.addThreadEvent(GBACPU::START);
		}

		ImGui::Separator();
		if (ImGui::BeginMenu("Audio Channels")) {
			ImGui::MenuItem("Channel 1", nullptr, &GBA.apu.ch1OverrideEnable);
			ImGui::MenuItem("Channel 2", nullptr, &GBA.apu.ch2OverrideEnable);
			ImGui::MenuItem("Channel 3", nullptr, &GBA.apu.ch3OverrideEnable);
			ImGui::MenuItem("Channel 4", nullptr, &GBA.apu.ch4OverrideEnable);
			ImGui::MenuItem("Channel A", nullptr, &GBA.apu.chAOverrideEnable);
			ImGui::MenuItem("Channel B", nullptr, &GBA.apu.chBOverrideEnable);

			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Debug")) {
		ImGui::MenuItem("Debug CPU", nullptr, &showCpuDebug);
		ImGui::MenuItem("System Log", nullptr, &showSystemLog);
		ImGui::MenuItem("Memory Editor", nullptr, &showMemEditor);
#if BUILD_WITH_PPUDEBUG
		ImGui::MenuItem("Inspect Layers", nullptr, &showLayerView);
		ImGui::MenuItem("View Tiles", nullptr, &showTiles);
		ImGui::MenuItem("View Palettes", nullptr, &showPalette);
#endif
		ImGui::Separator();
		ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow);

		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

void romInfoWindow() {
	ImGui::Begin("ROM Info", &showRomInfo);

	std::string saveTypeString;
	switch (GBA.saveType) {
	case GameBoyAdvance::UNKNOWN:
		saveTypeString = "Unknown";
		break;
	case GameBoyAdvance::EEPROM_512B:
		saveTypeString = "512 byte EEPROM";
		break;
	case GameBoyAdvance::EEPROM_8K:
		saveTypeString = "8 kilobyte EEPROM";
		break;
	case GameBoyAdvance::SRAM_32K:
		saveTypeString = "32 kilobyte SRAM";
		break;
	case GameBoyAdvance::FLASH_128K:
		saveTypeString = "128 kilobyte Flash";
		break;
	}

	ImGui::Text("ROM File:  %s", argRomFilePath.c_str());
	ImGui::Text("BIOS File:  %s", argBiosFilePath.c_str());
	ImGui::Text("Save Type:  %s", saveTypeString.c_str());

	ImGui::End();
}

void noBiosWindow() {
	// Center window
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5, 0.5));
	ImGui::Begin("No BIOS Selected");

	ImGui::Text("You have chosen a ROM, but no BIOS.\nWould you like to start now using the HLE BIOS or wait until one is selected?");
	ImGui::Text("Note: The HLE BIOS is very much a work in progress and likely will not work correctly.");

	if (ImGui::Button("Wait"))
		showNoBios = false;
	ImGui::SameLine();
	if (ImGui::Button("Continue")) {
		showNoBios = false;
		loadRom();
	}

	ImGui::End();
}

void cpuDebugWindow() {
	ImGui::Begin("Debug CPU", &showCpuDebug);

	if (ImGui::Button("Reset"))
		GBA.cpu.addThreadEvent(GBACPU::RESET);
	ImGui::SameLine();
	if (GBA.cpu.running) {
		if (ImGui::Button("Pause"))
			GBA.cpu.addThreadEvent(GBACPU::STOP, (u64)0);
	} else {
		if (ImGui::Button("Unpause"))
			GBA.cpu.addThreadEvent(GBACPU::START);
	}

	ImGui::Spacing();
	if (ImGui::Button("Step")) {
		// Add events the hard way so mutex doesn't have to be unlocked
		GBA.cpu.threadQueueMutex.lock();
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::START, 0, nullptr});
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::STOP, 1, nullptr});
		GBA.cpu.threadQueueMutex.unlock();
	}

	ImGui::Separator();
	std::string tmp = disassembler.disassemble(GBA.cpu.reg.R[15] - (GBA.cpu.reg.thumbMode ? 4 : 8), GBA.cpu.pipelineOpcode3, GBA.cpu.reg.thumbMode);
	ImGui::Text("Current Opcode:  %s", tmp.c_str());
	ImGui::Spacing();
	ImGui::Text("r0:  %08X", GBA.cpu.reg.R[0]);
	ImGui::Text("r1:  %08X", GBA.cpu.reg.R[1]);
	ImGui::Text("r2:  %08X", GBA.cpu.reg.R[2]);
	ImGui::Text("r3:  %08X", GBA.cpu.reg.R[3]);
	ImGui::Text("r4:  %08X", GBA.cpu.reg.R[4]);
	ImGui::Text("r5:  %08X", GBA.cpu.reg.R[5]);
	ImGui::Text("r6:  %08X", GBA.cpu.reg.R[6]);
	ImGui::Text("r7:  %08X", GBA.cpu.reg.R[7]);
	ImGui::Text("r8:  %08X", GBA.cpu.reg.R[8]);
	ImGui::Text("r9:  %08X", GBA.cpu.reg.R[9]);
	ImGui::Text("r10: %08X", GBA.cpu.reg.R[10]);
	ImGui::Text("r11: %08X", GBA.cpu.reg.R[11]);
	ImGui::Text("r12: %08X", GBA.cpu.reg.R[12]);
	ImGui::Text("r13: %08X", GBA.cpu.reg.R[13]);
	ImGui::Text("r14: %08X", GBA.cpu.reg.R[14]);
	ImGui::Text("r15: %08X", GBA.cpu.reg.R[15]);
	ImGui::Text("CPSR: %08X", GBA.cpu.reg.CPSR);

	ImGui::Spacing();
	bool imeTmp = GBA.cpu.IME;
	ImGui::Checkbox("IME", &imeTmp);
	ImGui::SameLine();
	ImGui::Text("IE: %04X", GBA.cpu.IE);
	ImGui::SameLine();
	ImGui::Text("IF: %04X", GBA.cpu.IF);

	ImGui::Spacing();
	if (ImGui::Button("Show System Log"))
		showSystemLog = true;

	ImGui::End();
}

void systemLogWindow() {
	static bool shouldAutoscroll = true;

	ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("System Log", &showSystemLog);

	ImGui::Checkbox("Trace Instructions", (bool *)&GBA.cpu.traceInstructions);
	ImGui::SameLine();
	ImGui::Checkbox("Log Interrupts", (bool *)&GBA.cpu.logInterrupts);
	ImGui::SameLine();
	ImGui::Checkbox("Log Flash Commands", (bool *)&GBA.logFlash);
	ImGui::SameLine();
	ImGui::Checkbox("Log DMAs", (bool *)&GBA.dma.logDma);

	ImGui::Spacing();
	ImGui::Checkbox("Auto-scroll", &shouldAutoscroll);
	ImGui::SameLine();
	if (ImGui::Button("Clear Log")) {
		GBA.cpu.addThreadEvent(GBACPU::CLEAR_LOG);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Log")) {
		std::ofstream logFileStream{"log", std::ios::trunc};
		logFileStream << GBA.log.str();
		logFileStream.close();
	}

	if (ImGui::TreeNode("Disassembler Options")) {
		ImGui::Checkbox("Show AL Condition", (bool *)&disassembler.options.showALCondition);
		ImGui::Checkbox("Always Show S Bit", (bool *)&disassembler.options.alwaysShowSBit);
		ImGui::Checkbox("Show Operands in Hex", (bool *)&disassembler.options.printOperandsHex);
		ImGui::Checkbox("Show Addresses in Hex", (bool *)&disassembler.options.printAddressesHex);
		ImGui::Checkbox("Simplify Register Names", (bool *)&disassembler.options.simplifyRegisterNames);
		ImGui::Checkbox("Simplify LDM and STM to PUSH and POP", (bool *)&disassembler.options.simplifyPushPop);
		ImGui::Checkbox("Use Alternative Stack Suffixes for LDM and STM", (bool *)&disassembler.options.ldmStmStackSuffixes);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Separator();

	if (ImGui::TreeNode("Log")) {
		ImGui::BeginChild("Debug CPU", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(GBA.log.str().c_str());
		if (shouldAutoscroll)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::End();
}

void romFileDialog() {
	nfdfilteritem_t filter[1] = {{"Game Boy Advance ROM", "gba,bin"}};
	NFD::UniquePath nfdRomFilePath;
	nfdresult_t nfdResult = NFD::OpenDialog(nfdRomFilePath, filter, 1);
	if (nfdResult == NFD_OKAY) {
		argRomGiven = true;
		argRomFilePath = nfdRomFilePath.get();
		if (!argBiosGiven) {
			showNoBios = true;
		} else {
			loadRom();
		}
	} else if (nfdResult != NFD_CANCEL) {
		printf("Error: %s\n", NFD::GetError());
	}
}

void biosFileDialog() {
	nfdfilteritem_t filter[1] = {{"Game Boy Advance BIOS", "bin"}};
	NFD::UniquePath nfdBiosFilePath;
	nfdresult_t nfdResult = NFD::OpenDialog(nfdBiosFilePath, filter, 1);
	if (nfdResult == NFD_OKAY) {
		argBiosGiven = true;
		argBiosFilePath = nfdBiosFilePath.get();
		showNoBios = false;
		if (argRomGiven)
			loadRom();
	} else if (nfdResult != NFD_CANCEL) {
		printf("Error: %s\n", NFD::GetError());
	}
}

void memEditorWindow() {
	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor", &showMemEditor);

	// I *may* have straight copied this text from GBATEK
	if (ImGui::BeginCombo("Location", "Jump to Memory Range")) {
		if (ImGui::MenuItem("BIOS - System ROM (0x0000000-0x0003FFF)"))
			memEditor.GotoAddrAndHighlight(0x0000000, 0x0000000);
		if (ImGui::MenuItem("WRAM - On-board Work RAM (0x2000000-0x203FFFF)"))
			memEditor.GotoAddrAndHighlight(0x2000000, 0x2000000);
		if (ImGui::MenuItem("WRAM - On-chip Work RAM (0x3000000-0x3007FFF)"))
			memEditor.GotoAddrAndHighlight(0x3000000, 0x3000000);
		if (ImGui::MenuItem("I/O Registers (0x4000000-0x4000209)"))
			memEditor.GotoAddrAndHighlight(0x4000000, 0x4000000);
		if (ImGui::MenuItem("BG/OBJ Palette RAM (0x5000000-0x50003FF)"))
			memEditor.GotoAddrAndHighlight(0x5000000, 0x5000000);
		if (ImGui::MenuItem("VRAM - Video RAM (0x6000000-0x6017FFF)"))
			memEditor.GotoAddrAndHighlight(0x6000000, 0x6000000);
		if (ImGui::MenuItem("OAM - OBJ Attributes (0x7000000-0x70003FF)"))
			memEditor.GotoAddrAndHighlight(0x7000000, 0x7000000);
		if (ImGui::MenuItem("Game Pak ROM (0x8000000-0x9FFFFFF)"))
			memEditor.GotoAddrAndHighlight(0x8000000, 0x9000000);
		if (ImGui::MenuItem("Game Pak SRAM (0xE000000-0xE00FFFF)"))
			memEditor.GotoAddrAndHighlight(0xE000000, 0xE000000);

		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Unrestricted Writes", &memEditorUnrestrictedWrites);

	memEditor.DrawContents(nullptr, 0x10000000);

	ImGui::End();
}

ImU8 memEditorRead(const ImU8* data, size_t off) {
	return GBA.readDebug((u32)off);
}

void memEditorWrite(ImU8* data, size_t off, ImU8 d) {
	GBA.writeDebug((u32)off, d, memEditorUnrestrictedWrites);
}

bool memEditorHighlight(const ImU8* data, size_t off) {
	switch (off) {
	case 0x0000000 ... 0x0003FFF:
	case 0x2000000 ... 0x203FFFF:
	case 0x3000000 ... 0x3007FFF:
	case 0x4000000 ... 0x4000209:
	case 0x5000000 ... 0x50003FF:
	case 0x6000000 ... 0x6017FFF:
	case 0x7000000 ... 0x70003FF:
	case 0x8000000 ... 0x9FFFFFF:
	case 0xE000000 ... 0xE00FFFF:
		return true;
	default:
		return false;
	}
}

u32 RGB5A1toRGBA8(u16 inRGB5A1) {
    u8 r = u8(float((inRGB5A1 & 0b0000000000011111) >> 0U)  / 32.0f * 255.0);
    u8 g = u8(float((inRGB5A1 & 0b0000001111100000) >> 5U)  / 32.0f * 255.0);
    u8 b = u8(float((inRGB5A1 & 0b0111110000000000) >> 10U) / 32.0f * 255.0);
    return r | g << 8 | b << 16 | 0xFF << 24;
}
