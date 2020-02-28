#ifndef NES_6502_H
#define NES_6502_H
#include <stdint.h>

void reset_6502();
void init_6502();
void reset_6502();
void run_6502();
uint64_t cycles_6502();
void status_6502();
void interrupt_6502();

#endif // NES_6502_H