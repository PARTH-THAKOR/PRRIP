#include "cache.h"
#include "ooo_cpu.h"

// Used this refrence to implement DRRIP
// DRRIP adapted from ChampSim's replacement/drrip and inc/msl/stat_methods.h.
// https://github.com/ChampSim/ChampSim/tree/master/replacement/drrip

#define maxRRPV 3  // 2bit RRIP
#define psel_max 1023  // maximum range of psel counter
#define sample_rate 8  // used for leader set selection
#define BRRIP_MAX 32  // used for giving the probablity to brrip
#define HISTORY_TABLE_SIZE 256  // size of the history table of evicted blocks
#define POLICY_WINDOW 1000  // phase size
#define MIN_HISTORY_LOOKUPS 16  // atlest this many history lookups for reuse calculation in table

// RRPV SECTION
static uint32_t l2_rrpv[L2C_SET][L2C_WAY];

// DRRIP SECTION
static uint32_t psel[NUM_CPUS];
static uint32_t brrip_counter = 0; // used for occasionaly insert at rrpv 2

// PRRIP SECTION

// history table of evicted block addresses
static uint64_t history[3][HISTORY_TABLE_SIZE];

// used to check that block is present or not
static bool history_valid[3][HISTORY_TABLE_SIZE];

// track the index of next location in table in circular manner
static uint32_t history_next_index[3];

// counter to track the no of lookups of each class
static uint64_t history_lookups[3];

// counter to track the no of hits of each class
static uint64_t history_hits[3];

// reuse rates from total history hits and lookups till last update
static double reuse_rate[3];

// track the instruction count of retired instruction till last phase
static uint64_t previous_update_instruction = 0;

// save evicted block in its class table, circular index gives FIFO order
static void add_to_history(BLOCK_CLASS bclass, uint64_t address)
{
    int category = (int)bclass;
    uint32_t index = history_next_index[category];
    history[category][index] = address;
    history_valid[category][index] = true;
    history_next_index[category] = (index + 1) % HISTORY_TABLE_SIZE;
}

// check incoming block in its class history and count its reuse
static bool find_in_history(BLOCK_CLASS bclass, uint64_t address)
{
    int category = (int)bclass;
    history_lookups[category]++;
    bool found = false;
    for (uint32_t index = 0; index < HISTORY_TABLE_SIZE; index++) {
        if (history_valid[category][index] && (history[category][index] == address)) {
            history_hits[category]++;
            // remove this entry so same eviction is not counted again
            history_valid[category][index] = false;
            found = true;
            break;
        }
    }
    return found;
}

// update after POLICY_WINDOW retired instructions, counters are not reset
static void update_reuse_rate()
{
    uint64_t retired = ooo_cpu[0].num_retired;
    if (retired - previous_update_instruction < POLICY_WINDOW)
        return;

    for (int category = 0; category < 3; category++) {
        if (history_lookups[category] >= MIN_HISTORY_LOOKUPS)
            reuse_rate[category] = (double)history_hits[category] / history_lookups[category];
    }
    previous_update_instruction = retired;
}

// get highest class reuse rate only when enough lookups are available
static double highest_reuse_rate()
{
    double highest = 0;
    for (int category = 0; category < 3; category++) {
        if (history_lookups[category] >= MIN_HISTORY_LOOKUPS && (reuse_rate[category] > highest))
            highest = reuse_rate[category];
    }
    return highest;
}

// category 0 is BRRIP leader, 1 is reuse leader, rest are followers
// for 1024 sets this gives 128 leaders each and 768 followers
static uint32_t set_category(uint32_t set)
{
    uint32_t low = set % sample_rate;
    uint32_t high = (set / sample_rate) % sample_rate;
    return (sample_rate + low - high) % sample_rate;
}

// initialize RRPVs and start PSEL from middle
void CACHE::l2c_initialize_replacement()
{
    cout << "Initialize PRRIP2 state for L2C" << endl;

    for (uint32_t set = 0; set < L2C_SET; set++)
        for (uint32_t way = 0; way < L2C_WAY; way++)
            l2_rrpv[set][way] = 0;

    for (uint32_t cpu = 0; cpu < NUM_CPUS; cpu++)
        psel[cpu] = psel_max / 2;
    brrip_counter = 0;
    previous_update_instruction = 0;

}

// among RRPV 3 blocks choose the class with lowest reuse
// this victim rule is same for leader sets and follower sets
uint32_t CACHE::l2c_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    while (1)
    {
        int victim = -1;
        bool enough_samples = true;
        for (int i=0; i<L2C_WAY; i++) {
            if (l2_rrpv[set][i] == maxRRPV) {
                if (victim == -1)
                    victim = i;
                if (history_lookups[(int)current_set[i].bclass] < MIN_HISTORY_LOOKUPS)
                    enough_samples = false;
            }
        }
        // if any candidate class has less samples, keep first RRPV 3 block
        if (victim != -1) {
            if (enough_samples) {
                for (int i=0; i<L2C_WAY; i++) {
                    if (l2_rrpv[set][i] == maxRRPV && (reuse_rate[(int)current_set[i].bclass] < reuse_rate[(int)current_set[victim].bclass]))
                        victim = i;
                }
            }
            return victim;
        }

        // no block at RRPV 3, increase all RRPVs and check again
        for (int i=0; i<L2C_WAY; i++)
            l2_rrpv[set][i]++;
    }

    assert(0);
    return 0;
}

// on hit promote to 0, on miss select insertion using reuse or BRRIP
void CACHE::l2c_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    update_reuse_rate();

    // writeback hit and miss both get RRPV 2, no lookup or PSEL update
    if (type == WRITEBACK) {
        if (!hit && block[set][way].valid)
            add_to_history(block[set][way].bclass, block[set][way].address);
        l2_rrpv[set][way] = maxRRPV - 1;
        return;
    }

    if (hit) {
        l2_rrpv[set][way] = 0;
        return;
    }

    // use block address, incoming instruction class comes from fill packet
    uint64_t address = full_addr >> LOG2_BLOCK_SIZE;
    BLOCK_CLASS bclass = BLOCK_CLASS::DATA;
    if (type == LOAD_TRANSLATION || type == PREFETCH_TRANSLATION || type == TRANSLATION_FROM_L1D)
        bclass = BLOCK_CLASS::PTE;
    else if (type == LOAD || type == PREFETCH) {
        if (MSHR.entry[MSHR.next_fill_index].instruction)
            bclass = BLOCK_CLASS::INSTRUCTION;
    }

    // check incoming address before adding victim, FIFO may overwrite that entry
    bool history_hit = find_in_history(bclass, address);

    if (block[set][way].valid)
        add_to_history(block[set][way].bclass, block[set][way].address);

    // history is tracked for all sets, PSEL selects insertion only for followers
    uint32_t category = set_category(set);
    bool use_brrip;
    if (category == 0)
        use_brrip = true;
    else if (category == 1)
        use_brrip = false;
    else
        use_brrip = psel[cpu] >= psel_max / 2;

    // BRRIP inserts at 3, every BRRIP_MAX insertion gets 2
    if (use_brrip) {
        l2_rrpv[set][way] = maxRRPV;
        brrip_counter++;
        if (brrip_counter == BRRIP_MAX) {
            brrip_counter = 0;
            l2_rrpv[set][way] = maxRRPV - 1;
        }
    } else {
        // history hit gets 0, highest reuse class gets 1, rest get 2
        double highest = highest_reuse_rate();
        if (history_hit)
            l2_rrpv[set][way] = 0;
        else if (history_lookups[(int)bclass] >= MIN_HISTORY_LOOKUPS && highest > 0 && (reuse_rate[(int)bclass] == highest))
            l2_rrpv[set][way] = 1;
        else
            l2_rrpv[set][way] = maxRRPV - 1;
    }

    // only leader misses change PSEL, followers and hits do not change it
    if (category == 0 && psel[cpu] > 0)
        psel[cpu]--;
    else if (category == 1 && psel[cpu] < psel_max)
        psel[cpu]++;
}

// print class history counters
void CACHE::l2c_replacement_final_stats()
{
    const char* names[3] = {"Data", "Instruction", "PTE"};
    cout << "Class\tLookups\tHistory hits\tReuse (%)" << endl;
    for (int category = 0; category < 3; category++) {
        cout << names[category] << '\t' << history_lookups[category]
             << '\t' << history_hits[category]
             << '\t' << reuse_rate[category] * 100 << endl;
    }
}
