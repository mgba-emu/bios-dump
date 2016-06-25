#include <gba_console.h>
#include <gba_systemcalls.h>
#include <stdio.h>
#include <string.h>

IWRAM_DATA uint8_t out[0x4000];
char savetype[] = "SRAM_V123"; // So that save tools can figure out the format

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
		"orr r3, r0, #0x4000 \n"
		"0: \n"
		"ldrb r2, [r11], #1 \n"
		"strb r2, [r0], #1 \n"
		"cmp r0, r3 \n"
		"blt 0b \n"
	: : : "r0", "r1", "r2", "r3", "r11", "r12", "memory");
	printf("Done dumping!");
}
