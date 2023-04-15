
#include "timer.hpp"
#include "gba.hpp"
#include <cstdio>

GBATIMER::GBATIMER(GameBoyAdvance& bus_) : bus(bus_) {
	reset();
}

void GBATIMER::reset() {
	TIM0D = TIM0CNT = initialTIM0D = 0;
	TIM1D = TIM1CNT = initialTIM1D = 0;
	TIM2D = TIM2CNT = initialTIM2D = 0;
	TIM3D = TIM3CNT = initialTIM3D = 0;
}

const int prescalerMasks[4] = {1, 64, 256, 1024};

void GBATIMER::checkOverflowEvent(void *object) {
	static_cast<GBATIMER *>(object)->checkOverflow();
}

void GBATIMER::checkOverflow() {
	bool previousOverflow = false;
	if (tim0Enable) {
		if (getDValue<0>() > 0xFFFF) { // Overflow
			if (tim0Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER0);

			TIM0D = initialTIM0D;
			tim0Timestamp = bus.cpu.currentTime;
			bus.cpu.addEvent((((0x10000 - TIM0D) * prescalerMasks[tim0Frequency]) + (tim0Timestamp & ~(prescalerMasks[tim0Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);

			bus.apu.onTimer(0);
			previousOverflow = true;
		} else {
			previousOverflow = false;
		}
	}
	if (tim1Enable) {
		if (getDValue<1>() > 0xFFFF) { // Overflow
			if (tim1Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER1);

			TIM1D = initialTIM1D;
			tim1Timestamp = bus.cpu.currentTime;
			//bus.cpu.addEvent((0x10000 - TIM1D) * (tim1Frequency ? (16 << (2 * tim1Frequency)) : 1), &checkOverflowEvent, this);
			bus.cpu.addEvent((((0x10000 - TIM1D) * prescalerMasks[tim1Frequency]) + (tim1Timestamp & ~(prescalerMasks[tim1Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);
			bus.apu.onTimer(1);

			previousOverflow = true;
		} else if (tim1Cascade && previousOverflow) { // Cascade
			if (++TIM1D == 0) { // Cascade Overflow
				if (tim1Irq)
					bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER1);

				bus.apu.onTimer(1);
				previousOverflow = true;
			}
		} else {
			previousOverflow = false;
		}
	}
	if (tim2Enable) {
		if (getDValue<2>() > 0xFFFF) { // Overflow
			if (tim2Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER2);

			TIM2D = initialTIM2D;
			tim2Timestamp = bus.cpu.currentTime;
			//bus.cpu.addEvent((0x10000 - TIM2D) * (tim2Frequency ? (16 << (2 * tim2Frequency)) : 1), &checkOverflowEvent, this);
			bus.cpu.addEvent((((0x10000 - TIM2D) * prescalerMasks[tim2Frequency]) + (tim2Timestamp & ~(prescalerMasks[tim2Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);

			previousOverflow = true;
		} else if (tim2Cascade && previousOverflow) { // Cascade
			if (++TIM2D == 0) { // Cascade Overflow
				if (tim2Irq)
					bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER2);

				previousOverflow = true;
			}
		} else {
			previousOverflow = false;
		}
	}
	if (tim3Enable) {
		if ((getDValue<3>() > 0xFFFF) && !tim3Cascade) { // Overflow
			if (tim3Irq)
				bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER3);

			TIM3D = initialTIM3D;
			tim3Timestamp = bus.cpu.currentTime;
			//bus.cpu.addEvent((0x10000 - TIM3D) * (tim3Frequency ? (16 << (2 * tim3Frequency)) : 1), &checkOverflowEvent, this);
			bus.cpu.addEvent((((0x10000 - TIM3D) * prescalerMasks[tim3Frequency]) + (tim3Timestamp & ~(prescalerMasks[tim3Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);
		} else if (tim3Cascade && previousOverflow) { // Cascade
			if (++TIM3D == 0) { // Cascade Overflow
				if (tim3Irq)
					bus.cpu.requestInterrupt(GBACPU::IRQ_TIMER3);
			}
		}
	}
}

template <int timer>
u64 GBATIMER::getDValue() {
	switch (timer) {
	case 0:
		//return TIM0D + (tim0Enable * ((bus.cpu.currentTime - tim0Timestamp) / (tim0Frequency ? (16 << (2 * tim0Frequency)) : 1)));
		return TIM0D + (tim0Enable * ((bus.cpu.currentTime - (tim0Timestamp & ~(prescalerMasks[tim0Frequency] - 1))) / prescalerMasks[tim0Frequency]));
	case 1:
		//return TIM1D + ((tim1Enable && !tim1Cascade) * ((bus.cpu.currentTime - tim1Timestamp) / (tim1Frequency ? (16 << (2 * tim1Frequency)) : 1)));
		return TIM1D + (tim1Enable * !tim1Cascade * ((bus.cpu.currentTime - (tim1Timestamp & ~(prescalerMasks[tim1Frequency] - 1))) / prescalerMasks[tim1Frequency]));
	case 2:
		//return TIM2D + ((tim2Enable && !tim2Cascade) * ((bus.cpu.currentTime - tim2Timestamp) / (tim2Frequency ? (16 << (2 * tim2Frequency)) : 1)));
		return TIM2D + (tim2Enable * !tim2Cascade * ((bus.cpu.currentTime - (tim2Timestamp & ~(prescalerMasks[tim2Frequency] - 1))) / prescalerMasks[tim2Frequency]));
	case 3:
		//return TIM3D + ((tim3Enable && !tim3Cascade) * ((bus.cpu.currentTime - tim3Timestamp) / (tim3Frequency ? (16 << (2 * tim3Frequency)) : 1)));
		return TIM3D + (tim3Enable * !tim3Cascade * ((bus.cpu.currentTime - (tim3Timestamp & ~(prescalerMasks[tim3Frequency] - 1))) / prescalerMasks[tim3Frequency]));
	}
}

u8 GBATIMER::readIO(u32 address) {
	switch (address) {
	case 0x4000100:
		return (u8)getDValue<0>();
	case 0x4000101:
		return (u8)(getDValue<0>() >> 8);
	case 0x4000102:
		return (u8)TIM0CNT;
	case 0x4000103:
		return 0;
	case 0x4000104:
		return (u8)getDValue<1>();
	case 0x4000105:
		return (u8)(getDValue<1>() >> 8);
	case 0x4000106:
		return (u8)TIM1CNT;
	case 0x4000107:
		return 0;
	case 0x4000108:
		return (u8)getDValue<2>();
	case 0x4000109:
		return (u8)(getDValue<2>() >> 8);
	case 0x400010A:
		return (u8)TIM2CNT;
	case 0x400010B:
		return 0;
	case 0x400010C:
		return (u8)getDValue<3>();
	case 0x400010D:
		return (u8)(getDValue<3>() >> 8);
	case 0x400010E:
		return (u8)TIM3CNT;
	case 0x400010F:
		return 0;
	default:
		return bus.openBus<u8>(address);
	}
}

void GBATIMER::writeIO(u32 address, u8 value) {
	switch (address) {
	case 0x4000100:
		initialTIM0D = (initialTIM0D & 0xFF00) | value;
		break;
	case 0x4000101:
		initialTIM0D = (initialTIM0D & 0x00FF) | (value << 8);
		break;
	case 0x4000102:
		if ((value & 0x80) && (!tim0Enable || ((value & 0x03) != tim0Frequency))) { // Enabling the timer or changing frequency
			TIM0D = initialTIM0D;
			tim0Timestamp = bus.cpu.currentTime + 2;
			bus.cpu.addEvent((((0x10000 - TIM0D) * prescalerMasks[tim0Frequency]) + (tim0Timestamp & ~(prescalerMasks[tim0Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);
		}
		if (!(value & 0x80) && tim0Enable) // Disabling the timer
			TIM0D = getDValue<0>();

		TIM0CNT = value & 0xC3;
		break;
	case 0x4000104:
		initialTIM1D = (initialTIM1D & 0xFF00) | value;
		break;
	case 0x4000105:
		initialTIM1D = (initialTIM1D & 0x00FF) | (value << 8);
		break;
	case 0x4000106:
		if ((value & 0x80) && (!tim1Enable || ((value & 0x03) != tim1Frequency))) { // Enabling the timer or changing frequency
			TIM1D = initialTIM1D;
			tim1Timestamp = bus.cpu.currentTime + 2;
			bus.cpu.addEvent((((0x10000 - TIM1D) * prescalerMasks[tim1Frequency]) + (tim1Timestamp & ~(prescalerMasks[tim1Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);
		}
		if (!(value & 0x80) && tim1Enable) // Disabling the timer
			TIM1D = getDValue<1>();

		TIM1CNT = value & 0xC7;
		break;
	case 0x4000108:
		initialTIM2D = (initialTIM2D & 0xFF00) | value;
		break;
	case 0x4000109:
		initialTIM2D = (initialTIM2D & 0x00FF) | (value << 8);
		break;
	case 0x400010A:
		if ((value & 0x80) && (!tim2Enable || ((value & 0x03) != tim2Frequency))) { // Enabling the timer or changing frequency
			TIM2D = initialTIM2D;
			tim2Timestamp = bus.cpu.currentTime + 2;
			bus.cpu.addEvent((((0x10000 - TIM2D) * prescalerMasks[tim2Frequency]) + (tim2Timestamp & ~(prescalerMasks[tim2Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);
		}
		if (!(value & 0x80) && tim2Enable) // Disabling the timer
			TIM2D = getDValue<2>();

		TIM2CNT = value & 0xC7;
		break;
	case 0x400010C:
		initialTIM3D = (initialTIM3D & 0xFF00) | value;
		break;
	case 0x400010D:
		initialTIM3D = (initialTIM3D & 0x00FF) | (value << 8);
		break;
	case 0x400010E:
		if ((value & 0x80) && (!tim3Enable || ((value & 0x03) != tim3Frequency))) { // Enabling the timer or changing frequency
			TIM3D = initialTIM3D;
			tim3Timestamp = bus.cpu.currentTime + 2;
			bus.cpu.addEvent((((0x10000 - TIM3D) * prescalerMasks[tim3Frequency]) + (tim3Timestamp & ~(prescalerMasks[tim3Frequency] - 1))) - bus.cpu.currentTime, &checkOverflowEvent, this);
		}
		if (!(value & 0x80) && tim3Enable) // Disabling the timer
			TIM3D = getDValue<3>();

		TIM3CNT = value & 0xC7;
		break;
	}
}