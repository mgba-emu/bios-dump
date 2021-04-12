#include <gba_console.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_timers.h>
#include <stdio.h>
#include <string.h>
#ifdef BIOS_CALC_SHA256
#include "Sha256.h"
#endif

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
	iprintf("Scanning stack for pointers...\n");
	for (; stack < top; ++stack) {
		u32 val = *stack;
		u32 sDelta = val - (u32) bbSource;
		u32 dDelta = val - (u32) out;
		if (dDelta > 0 && dDelta < sizeof(out)) {
			iprintf("Dest pointer: %8p\n", stack);
			iprintf("Current value: %8lX\n", *stack);
			*stack = (u32) out;
		}
		if (dDelta == sizeof(out)) {
			iprintf("Dest-end pointer: %8p\n", stack);
			iprintf("Current value: %8lX\n", *stack);
			*stack = (u32) out + sizeof(out);
		}
		if (sDelta > 0 && sDelta < sizeof(bbSource)) {
			iprintf("Source pointer: %8p\n", stack);
			iprintf("Current value: %8lX\n", *stack);
			*stack = 0;
		}
		if (sDelta == sizeof(bbSource)) {
			iprintf("Source-end pointer: %8p\n", stack);
			iprintf("Current value: %8lX\n", *stack);
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

#ifdef BIOS_CALC_SHA256
const u8 sha256_checksum_agb[SHA256_DIGEST_SIZE] = {
	0xfd, 0x25, 0x47, 0x72, 0x4b, 0x50, 0x5f, 0x48,
	0x7e, 0x6d, 0xcb, 0x29, 0xec, 0x2e, 0xcf, 0xf3,
	0xaf, 0x35, 0xa8, 0x41, 0xa7, 0x7a, 0xb2, 0xe8,
	0x5f, 0xd8, 0x73, 0x50, 0xab, 0xd3, 0x65, 0x70
};

const u8 sha256_checksum_ntr[SHA256_DIGEST_SIZE] = {
	0x78, 0x2e, 0xb3, 0x89, 0x42, 0x37, 0xec, 0x6a,
	0xa4, 0x11, 0xb7, 0x8f, 0xfe, 0xe1, 0x90, 0x78,
	0xba, 0xcf, 0x10, 0x41, 0x38, 0x56, 0xd3, 0x3c,
	0xda, 0x10, 0xb4, 0x4f, 0xd9, 0xc2, 0x85, 0x6b
};

IWRAM_DATA CSha256 sha256_data;

void calcSha256(void) {
	u8 checksum[SHA256_DIGEST_SIZE];
	int i;

	iprintf("Calculating SHA256...\n");

	Sha256_Init(&sha256_data);
	Sha256_Update(&sha256_data, out, sizeof(out));
	Sha256_Final(&sha256_data, checksum);

	iprintf("SHA256: ");
	for (i = 0; i < SHA256_DIGEST_SIZE; i++) {
		iprintf("%02x", checksum[i]);
	}
	iprintf("\nCPU %s\n",
		!memcmp(checksum, sha256_checksum_agb, SHA256_DIGEST_SIZE) ? "AGB" :
		!memcmp(checksum, sha256_checksum_ntr, SHA256_DIGEST_SIZE) ? "NTR" : "???");
	iprintf("\n");
}
#endif

int main() {
	consoleDemoInit();

	if (strcmp(savetype, "SRAM_V123") != 0) {
		iprintf("Cartridge error, continuing anyway\n");
	}
	*(vu8*) SRAM = 0x55;
	if (*(vu8*) SRAM != 0x55) {
		iprintf("Fatal SRAM error!\n");
		return 1;
	}

	u32 checksum = BiosCheckSum();
	iprintf("BIOS Checksum: %08lX\nCPU %s\n", checksum,
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
		iprintf("Calculated Checksum: %08lX\n", checksum2);
		if (checksum2 != checksum) {
			iprintf("Incorrect checksum!\n");
			iprintf("Trying again with different timing.\n");
			timing -= 0x10;
			continue;
		}
		break;
	}
#else
	dump();
#endif

#ifdef BIOS_WRITE_SRAM
	for (i = 0; i < sizeof(out); ++i) {
		((vu8*) SRAM)[i] = out[i];
	}
#endif

	iprintf("Done dumping!\n");

#ifdef BIOS_CALC_SHA256
	calcSha256();
#endif

	while (1) {
		VBlankIntrWait();
	}
}
