#include "phase_l2c.h"
#include "ooo_cpu.h"

bool l2_phase_enabled = false;

static const uint64_t phase_instructions = 1000;
static uint64_t previous_cycle = 0;
static uint64_t instruction_hits = 0, instruction_misses = 0;
static uint64_t data_hits = 0, data_misses = 0;
static uint64_t pte_hits = 0, pte_misses = 0;
static uint64_t previous_stlb_misses = 0;
static uint64_t previous_l1d_misses = 0;
static uint64_t previous_l1i_misses = 0;

void l2_phase_initialize()
{
    l2_phase_enabled = true;
}

void l2_phase_record(BLOCK_CLASS bclass, uint8_t hit)
{
    if (bclass == BLOCK_CLASS::INSTRUCTION) {
        if (hit) instruction_hits++;
        else instruction_misses++;
    } else if (bclass == BLOCK_CLASS::PTE) {
        if (hit) pte_hits++;
        else pte_misses++;
    } else {
        if (hit) data_hits++;
        else data_misses++;
    }
}

void l2_phase_retire()
{
    uint64_t instructions = ooo_cpu[0].num_retired - ooo_cpu[0].begin_sim_instr;
    if (instructions == 0 || instructions % phase_instructions != 0)
        return;

    if (instructions == phase_instructions) {
        previous_cycle = ooo_cpu[0].begin_sim_cycle;
        cout << "PHASE_START," << ooo_cpu[0].begin_sim_instr << endl;
    }

    uint64_t stlb_misses = ooo_cpu[0].STLB.MISS[LOAD_TRANSLATION];
    uint64_t l1d_misses = ooo_cpu[0].L1D.MISS[LOAD] + ooo_cpu[0].L1D.MISS[RFO];
    uint64_t l1i_misses = ooo_cpu[0].L1I.MISS[LOAD];

    cout << "PHASE_WINDOW," << instructions
         << ',' << current_core_cycle[0] - previous_cycle
         << ',' << instruction_hits << ',' << instruction_misses
         << ',' << data_hits << ',' << data_misses
         << ',' << pte_hits << ',' << pte_misses
         << ',' << stlb_misses - previous_stlb_misses
         << ',' << l1d_misses - previous_l1d_misses
         << ',' << l1i_misses - previous_l1i_misses << endl;

    previous_stlb_misses = stlb_misses;
    previous_l1d_misses = l1d_misses;
    previous_l1i_misses = l1i_misses;
    previous_cycle = current_core_cycle[0];
    instruction_hits = instruction_misses = 0;
    data_hits = data_misses = 0;
    pte_hits = pte_misses = 0;
}
