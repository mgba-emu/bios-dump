#include <gba_console.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_timers.h>
#include <stdio.h>
#include <string.h>

char savetype[] = "SRAM_V123"; // So that save tools can figure out the format

IWRAM_DATA u8 out[0x4000];

void dump(void) {
	__asm__ __volatile__(
		"mov r0, #0 \n"
		"ldr r11, =out \n"
		"orr r10, r11, #0x4000 \n"
		"mov r1, r11 \n"
		"ldr r12, =0xC14 \n" // CpuFastSet core
		"add lr, pc, #4 \n"
		"push {r4-r10,lr} \n"
		"bx r12 \n"
		"mov r0, #0xE000000 \n"
	: : : "r0", "r1", "r2", "r3", "r10", "r11", "r12", "lr", "memory");
}

#ifdef USE_BLACKBOX
EWRAM_DATA u32 bbSource[0x1000];

int timing = 0xFF00;

void bbTest(void) {
	REG_IE &= ~IRQ_TIMER0;
	REG_TM0CNT = 0;
	u32* stack = (u32*) 0x3007F00;
	u32* top = (u32*) 0x3007FE0;
	printf("Scanning stack for pointers...\n");
	for (; stack < top; ++stack) {
		u32 val = *stack;
		u32 sDelta = val - (u32) bbSource;
		u32 dDelta = val - (u32) out;
		if (dDelta > 0 && dDelta < sizeof(out)) {
			printf("Dest pointer: %8p\n", stack);
			printf("Current value: %8lX\n", *stack);
			*stack = (u32) out;
		}
		if (dDelta == sizeof(out)) {
			printf("Dest-end pointer: %8p\n", stack);
			printf("Current value: %8lX\n", *stack);
			*stack = (u32) out + sizeof(out);
		}
		if (sDelta > 0 && sDelta < sizeof(bbSource)) {
			printf("Source pointer: %8p\n", stack);
			printf("Current value: %8lX\n", *stack);
			*stack = 0;
		}
		if (sDelta == sizeof(bbSource)) {
			printf("Source-end pointer: %8p\n", stack);
			printf("Current value: %8lX\n", *stack);
			*stack = sizeof(out);
		}
	}
}

void blackBox(void) {
	irqInit();
	irqSet(IRQ_TIMER0, bbTest);
	REG_IME = 0;
	REG_IE |= IRQ_TIMER0;
	REG_IME = 1;
	REG_TM0CNT = 0xC00000 | timing;
	CpuFastSet(bbSource, out, sizeof(out) / 4);
}
#endif

int main() {
	consoleDemoInit();

	if (strcmp(savetype, "SRAM_V123") != 0) {
		printf("Cartridge error, continuing anyway\n");
	}
	*(vu8*) SRAM = 0x55;
	if (*(vu8*) SRAM != 0x55) {
		printf("Fatal SRAM error!\n");
		return 1;
	}

	u32 checksum = BiosCheckSum();
	printf("BIOS Checksum: %08lX\nCPU %s\n", checksum,
		checksum == 0xBAAE187F ? "AGB" :
		checksum == 0xBAAE1880 ? "NTR" : "???");

	size_t i;
#ifdef USE_BLACKBOX
	for (i = 0; i < 8; ++i) {
		blackBox();

		u32 checksum2 = 0;
		size_t c;
		for (c = 0; c < sizeof(out) / 4; ++c) {
			checksum2 += ((u32*) out)[c];
		}
		printf("Calculated Checksum: %08lX\n", checksum2);
		if (checksum2 != checksum) {
			printf("Incorrect checksum!\n");
			printf("Trying again with different timing.\n");
			timing -= 0x10;
			continue;
		}
		break;
	}
#else
	dump();
#endif

	for (i = 0; i < sizeof(out); ++i) {
		((vu8*) SRAM)[i] = out[i];
	}

	printf("Done dumping!");
	while (1) {
		VBlankIntrWait();
	}
}
