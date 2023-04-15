
#ifndef GBA_HPP
#define GBA_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <functional>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
//#include <unistd.h>
#include <vector>

#include <fmt/core.h>

#include "types.hpp"
#include "cpu.hpp"
#include "apu.hpp"
#include "dma.hpp"
#include "ppu.hpp"
#include "timer.hpp"

class GBACPU;
class GBAPPU;
class GameBoyAdvance {
public:
	GBACPU cpu;
	GBAAPU apu;
	GBADMA dma;
	GBAPPU ppu;
	GBATIMER timer;

	GameBoyAdvance();
	~GameBoyAdvance();
	void reset();

	bool searchRomForString(char *pattern, size_t patternSize);
	int loadBios(std::filesystem::path biosFilePath_);
	int loadRom(std::filesystem::path romFilePath_);
	void save();

	u8 readDebug(u32 address);
	template <typename T> T openBus(u32 address);
	template <typename T, bool code, bool rotate = true> u32 read(u32 address, bool sequential);
	u8 readIO(u32 address);
	void writeDebug(u32 address, u8 value, bool unrestricted);
	template <typename T> void write(u32 address, T value, bool sequential);
	void writeIO(u32 address, u8 value);

	bool forceNonSequential;
	void internalCycle(int cycles);

	bool prefetchRunning;
	int prefetchIndex;
	int prefetchWaitstate;
	int prefetchCycles;
	int prefetchLastAddress;
	void tickPrefetch(int cycles);
	bool checkPrefetch(u32 address, bool sequential);

	std::stringstream log;
	bool logFlash;

	enum {
		UNKNOWN,
		EEPROM_512B,
		EEPROM_8K,
		SRAM_32K,
		FLASH_128K
	} saveType;
	std::filesystem::path saveFilePath;
	enum {
		READY = 1 << 0,
		CMD_1 = 1 << 1,
		CMD_2 = 1 << 2,
		ERASE = 1 << 3,
		WRITE = 1 << 4,
		BANK = 1 << 5
	};
	int flashState;
	bool flashChipId;
	int flashBank;

	u32 biosOpenBusValue;
	u32 openBusValue;
	u8 ewram[0x40000];
	u8 iwram[0x8000];
	u16 KEYINPUT; // 0x4000130
	u16 KEYCNT; // 0x4000132
	bool POSTFLG; // 0x4000300

	union {
		struct {
			u16 sramWaitControl : 2;
			u16 ws0NonSequentialControl : 2;
			u16 ws0SequentialControl : 1;
			u16 ws1NonSequentialControl : 2;
			u16 ws1SequentialControl : 1;
			u16 ws2NonSequentialControl : 2;
			u16 ws2SequentialControl : 1;
			u16 phiControl : 2;
			u16 : 1;
			u16 prefetchBufferEnable : 1;
			u16 : 1;
		};
		u16 WAITCNT; // 0x4000204
	};
	union { // TODO: This is a stub
		struct {
			u32 biosSwap : 1;
			u32 : 4;
			u32 ewramEnable : 1;
			u32 : 18;
			u32 ewramWaitControl : 4;
			u32 : 4;
		};
		u32 InternalMemoryControl; // 0x4xx0800
	};
	int sramCycles;
	int wsNonSequentialCycles[3];
	int wsSequentialCycles[3];
	int ewramCycles;

	std::vector<u8> biosBuff;
	int romSize;
	std::vector<u8> romBuff;
	std::vector<u8> sram;
};

#endif