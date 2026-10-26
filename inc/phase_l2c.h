#include "block.h"

extern bool l2_phase_enabled;

void l2_phase_initialize();
void l2_phase_record(BLOCK_CLASS bclass, uint8_t hit);
void l2_phase_retire();
