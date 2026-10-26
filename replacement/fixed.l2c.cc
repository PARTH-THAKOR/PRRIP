#include "cache.h"
#include "phase_l2c.h"

void CACHE::l2c_initialize_replacement()
{
    l2_phase_initialize();
    cout << NAME << " has fixed class priority with LRU within each class" << endl;
}

uint32_t CACHE::l2c_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    // Highest to lowest priority. Change this order for each experiment.
    const BLOCK_CLASS priority[3] = {
        BLOCK_CLASS::DATA, BLOCK_CLASS::INSTRUCTION, BLOCK_CLASS::PTE
    };

    for (uint32_t way = 0; way < NUM_WAY; way++) {
        if (!current_set[way].valid)
            return way;
    }

    // Evict the oldest block from the lowest priority class present.
    for (int rank = 2; rank >= 0; rank--) {
        uint32_t victim = NUM_WAY;
        for (uint32_t way = 0; way < NUM_WAY; way++) {
            if (current_set[way].bclass == priority[rank]) {
                if (victim == NUM_WAY || current_set[way].lru > current_set[victim].lru)
                    victim = way;
            }
        }
        if (victim != NUM_WAY)
            return victim;
    }

    return lru_victim(cpu, instr_id, set, current_set, ip, full_addr, type);
}

void CACHE::l2c_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    if ((type == WRITEBACK) && ip)
        assert(0);

    if (all_warmup_complete > NUM_CPUS && !simulation_complete[0]) {
        BLOCK_CLASS bclass = BLOCK_CLASS::DATA;

        if (type == LOAD_TRANSLATION || type == PREFETCH_TRANSLATION || type == TRANSLATION_FROM_L1D) {
            bclass = BLOCK_CLASS::PTE;
        } else if (type == LOAD || type == PREFETCH) {
            if (hit) {
                bclass = block[set][way].bclass;
            } else {
                if (MSHR.entry[MSHR.next_fill_index].instruction)
                    bclass = BLOCK_CLASS::INSTRUCTION;
            }
        }

        l2_phase_record(bclass, hit);
    }

    // Original LRU behaviour.
    if (hit && type == WRITEBACK)
        return;

    lru_update(set, way);
}

void CACHE::l2c_replacement_final_stats()
{
}
