#include "cache.h"
#include "ooo_cpu.h"

static const uint8_t maxRRPV = 7;
static uint8_t l2_rrpv[L2C_SET][L2C_WAY];
static uint8_t class_rank[3];
static bool ranking_enabled = false;
static const uint64_t policy_window = 100000;
static uint64_t previous_instruction = 0;
static uint64_t previous_l1i_misses = 0;
static uint64_t previous_stlb_misses = 0;
static uint64_t window_accesses = 0;
static uint64_t instruction_accesses = 0;

void update_policy()
{
    if (all_warmup_complete <= NUM_CPUS || simulation_complete[0])
        return;

    uint64_t retired = ooo_cpu[0].num_retired - ooo_cpu[0].begin_sim_instr;
    if (retired - previous_instruction < policy_window)
        return;

    uint64_t l1i_misses = ooo_cpu[0].L1I.MISS[LOAD];
    uint64_t stlb_misses = ooo_cpu[0].STLB.MISS[LOAD_TRANSLATION];
    // Both cover the same instruction interval, so comparing counts equals comparing MPKI.
    uint64_t instruction_pressure = l1i_misses - previous_l1i_misses;
    uint64_t translation_pressure = stlb_misses - previous_stlb_misses;

    if (window_accesses > 0) {
        if (instruction_accesses == 0) {
            // No Instruction L2 events: P > D > I.
            class_rank[(int)BLOCK_CLASS::PTE] = 1;
            class_rank[(int)BLOCK_CLASS::DATA] = 2;
            class_rank[(int)BLOCK_CLASS::INSTRUCTION] = 3;
        } else if (instruction_pressure > translation_pressure) {
            // More Instruction pressure: I > P > D.
            class_rank[(int)BLOCK_CLASS::INSTRUCTION] = 1;
            class_rank[(int)BLOCK_CLASS::PTE] = 2;
            class_rank[(int)BLOCK_CLASS::DATA] = 3;
        } else {
            // More translation pressure, or a tie: P > I > D.
            class_rank[(int)BLOCK_CLASS::PTE] = 1;
            class_rank[(int)BLOCK_CLASS::INSTRUCTION] = 2;
            class_rank[(int)BLOCK_CLASS::DATA] = 3;
        }
        ranking_enabled = true;
    }

    previous_instruction = retired;
    previous_l1i_misses = l1i_misses;
    previous_stlb_misses = stlb_misses;
    window_accesses = instruction_accesses = 0;
}

void CACHE::l2c_initialize_replacement()
{
    cout << NAME << " has PRRIP replacement policy (initially 3-bit SRRIP)" << endl;
    ranking_enabled = false;
    previous_instruction = previous_l1i_misses = previous_stlb_misses = 0;
    window_accesses = instruction_accesses = 0;
    for (int category = 0; category < 3; category++)
        class_rank[category] = 0;

    for (uint32_t set = 0; set < L2C_SET; set++) {
        for (uint32_t way = 0; way < L2C_WAY; way++)
            l2_rrpv[set][way] = maxRRPV;
    }
}

uint32_t CACHE::l2c_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    update_policy();

    for (uint32_t way = 0; way < NUM_WAY; way++) {
        if (!current_set[way].valid)
            return way;
    }

    while (true) {
        if (!ranking_enabled) {
            for (uint32_t way = 0; way < NUM_WAY; way++) {
                if (l2_rrpv[set][way] == maxRRPV)
                    return way;
            }
        } else {
            // In RRPV 5-7, prefer rank 3, then rank 2, then rank 1.
            for (int rank = 3; rank >= 1; rank--) {
                uint32_t victim = NUM_WAY;
                for (uint32_t way = 0; way < NUM_WAY; way++) {
                    if (l2_rrpv[set][way] >= 5 && class_rank[(int)current_set[way].bclass] == rank) {
                        if (victim == NUM_WAY || l2_rrpv[set][way] > l2_rrpv[set][victim])
                            victim = way;
                    }
                }
                if (victim != NUM_WAY)
                    return victim;
            }
        }

        for (uint32_t way = 0; way < NUM_WAY; way++) {
            if (l2_rrpv[set][way] < maxRRPV)
                l2_rrpv[set][way]++;
        }
    }
}

void CACHE::l2c_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    if ((type == WRITEBACK) && ip)
        assert(0);

    update_policy();

    BLOCK_CLASS bclass = BLOCK_CLASS::DATA;
    if (hit) {
        bclass = block[set][way].bclass;
    } else if (type == LOAD_TRANSLATION || type == PREFETCH_TRANSLATION || type == TRANSLATION_FROM_L1D) {
        bclass = BLOCK_CLASS::PTE;
    } else if (type == LOAD || type == PREFETCH) {
        // A fill callback runs before the incoming block is installed.
        if (MSHR.entry[MSHR.next_fill_index].instruction)
            bclass = BLOCK_CLASS::INSTRUCTION;
    }

    if (all_warmup_complete > NUM_CPUS && !simulation_complete[0]) {
        window_accesses++;
        if (bclass == BLOCK_CLASS::INSTRUCTION)
            instruction_accesses++;
    }

    // Use 3-bit SRRIP until the first nonempty feedback window finishes.
    if (!ranking_enabled) {
        l2_rrpv[set][way] = hit ? 0 : maxRRPV - 1;
        return;
    }

    uint8_t rank = class_rank[(int)bclass];
    if (!hit) {
        // Rank 1 -> 3, rank 2 -> 5, rank 3 -> 7.
        l2_rrpv[set][way] = 2 * rank + 1;
    } else if (rank == 1) {
        l2_rrpv[set][way] = 0;
    } else if (rank == 2) {
        if (l2_rrpv[set][way] >= 2)
            l2_rrpv[set][way] -= 2;
        else
            l2_rrpv[set][way] = 0;
    } else {
        if (l2_rrpv[set][way] > 0)
            l2_rrpv[set][way]--;
    }
}

void CACHE::l2c_replacement_final_stats()
{
}
