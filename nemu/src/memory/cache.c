#include "memory/cache.h"
#include "memory/memory.h"

uint32_t dram_read(hwaddr_t addr, size_t len);
void dram_write(hwaddr_t addr, size_t len, uint32_t data);

/* L1 Cache Configuration */
#define L1_CACHE_SIZE (64 * 1024)
#define L1_CACHE_BLOCK_SIZE 64
#define L1_CACHE_WAYS 8

#define L1_CACHE_BLOCK_MASK (L1_CACHE_BLOCK_SIZE - 1)
#define L1_CACHE_LINE_COUNT (L1_CACHE_SIZE / L1_CACHE_BLOCK_SIZE)
#define L1_CACHE_SET_COUNT (L1_CACHE_LINE_COUNT / L1_CACHE_WAYS)

/* L2 Cache Configuration */
#define L2_CACHE_SIZE (4 * 1024 * 1024)
#define L2_CACHE_BLOCK_SIZE 64
#define L2_CACHE_WAYS 16

#define L2_CACHE_BLOCK_MASK (L2_CACHE_BLOCK_SIZE - 1)
#define L2_CACHE_LINE_COUNT (L2_CACHE_SIZE / L2_CACHE_BLOCK_SIZE)
#define L2_CACHE_SET_COUNT (L2_CACHE_LINE_COUNT / L2_CACHE_WAYS)

typedef struct {
    uint8_t data[L1_CACHE_BLOCK_SIZE];
    uint32_t tag;
    bool valid;
} L1_CacheLine;

typedef struct {
    L1_CacheLine lines[L1_CACHE_WAYS];
} L1_CacheSet;

typedef struct {
    uint8_t data[L2_CACHE_BLOCK_SIZE];
    uint32_t tag;
    bool valid;
    bool dirty;
} L2_CacheLine;

typedef struct {
    L2_CacheLine lines[L2_CACHE_WAYS];
} L2_CacheSet;

static L1_CacheSet l1_cache[L1_CACHE_SET_COUNT];
static L2_CacheSet l2_cache[L2_CACHE_SET_COUNT];
static uint32_t rand_state = 1;

static inline uint32_t next_rand(void) {
    rand_state = rand_state * 1103515245 + 12345;
    return rand_state;
}

/* L1 Cache Helpers */
static inline uint32_t l1_addr_tag(hwaddr_t addr) {
    return addr >> (__builtin_ctz(L1_CACHE_BLOCK_SIZE) + __builtin_ctz(L1_CACHE_SET_COUNT));
}

static inline uint32_t l1_addr_set(hwaddr_t addr) {
    return (addr >> __builtin_ctz(L1_CACHE_BLOCK_SIZE)) & (L1_CACHE_SET_COUNT - 1);
}

static inline uint32_t l1_addr_offset(hwaddr_t addr) {
    return addr & L1_CACHE_BLOCK_MASK;
}

/* L2 Cache Helpers */
static inline uint32_t l2_addr_tag(hwaddr_t addr) {
    return addr >> (__builtin_ctz(L2_CACHE_BLOCK_SIZE) + __builtin_ctz(L2_CACHE_SET_COUNT));
}

static inline uint32_t l2_addr_set(hwaddr_t addr) {
    return (addr >> __builtin_ctz(L2_CACHE_BLOCK_SIZE)) & (L2_CACHE_SET_COUNT - 1);
}

static inline uint32_t l2_addr_offset(hwaddr_t addr) {
    return addr & L2_CACHE_BLOCK_MASK;
}

/* L2 Cache Operations */
static L2_CacheLine *l2_select_line(L2_CacheSet *set, uint32_t tag) {
    L2_CacheLine *invalid = NULL;
    int i;
    for(i = 0; i < L2_CACHE_WAYS; i ++) {
        L2_CacheLine *line = &set->lines[i];
        if(line->valid && line->tag == tag) {
            return line;
        }
        if(!line->valid && invalid == NULL) {
            invalid = line;
        }
    }
    if(invalid) {
        return invalid;
    }
    uint32_t victim = next_rand() % L2_CACHE_WAYS;
    return &set->lines[victim];
}

static L2_CacheLine *l2_find_line(L2_CacheSet *set, uint32_t tag) {
    int i;
    for(i = 0; i < L2_CACHE_WAYS; i ++) {
        L2_CacheLine *line = &set->lines[i];
        if(line->valid && line->tag == tag) {
            return line;
        }
    }
    return NULL;
}

static void l2_writeback_line(L2_CacheLine *line, hwaddr_t block_addr) {
    uint32_t i;
    for(i = 0; i < L2_CACHE_BLOCK_SIZE; i += 4) {
        uint32_t val;
        memcpy(&val, line->data + i, 4);
        dram_write(block_addr + i, 4, val);
    }
}

static void l2_fill_line(L2_CacheLine *line, hwaddr_t block_addr, uint32_t tag) {
    uint32_t i;
    for(i = 0; i < L2_CACHE_BLOCK_SIZE; i += 4) {
        uint32_t val = dram_read(block_addr + i, 4);
        memcpy(line->data + i, &val, 4);
    }
    line->tag = tag;
    line->valid = true;
    line->dirty = false;
}

static uint8_t l2_read_byte(hwaddr_t addr) {
    uint32_t set_idx = l2_addr_set(addr);
    uint32_t tag = l2_addr_tag(addr);
    uint32_t offset = l2_addr_offset(addr);

    L2_CacheSet *set = &l2_cache[set_idx];
    L2_CacheLine *line = l2_find_line(set, tag);
    if(line == NULL) {
        hwaddr_t block_addr = addr & ~L2_CACHE_BLOCK_MASK;
        line = l2_select_line(set, tag);
        if(line->valid && line->dirty) {
            hwaddr_t victim_addr = (line->tag << (__builtin_ctz(L2_CACHE_BLOCK_SIZE) + __builtin_ctz(L2_CACHE_SET_COUNT))) | (set_idx << __builtin_ctz(L2_CACHE_BLOCK_SIZE));
            l2_writeback_line(line, victim_addr);
        }
        l2_fill_line(line, block_addr, tag);
    }
    return line->data[offset];
}

static void l2_write_byte(hwaddr_t addr, uint8_t data) {
    uint32_t set_idx = l2_addr_set(addr);
    uint32_t tag = l2_addr_tag(addr);
    uint32_t offset = l2_addr_offset(addr);

    L2_CacheSet *set = &l2_cache[set_idx];
    L2_CacheLine *line = l2_find_line(set, tag);
    if(line == NULL) {
        hwaddr_t block_addr = addr & ~L2_CACHE_BLOCK_MASK;
        line = l2_select_line(set, tag);
        if(line->valid && line->dirty) {
            hwaddr_t victim_addr = (line->tag << (__builtin_ctz(L2_CACHE_BLOCK_SIZE) + __builtin_ctz(L2_CACHE_SET_COUNT))) | (set_idx << __builtin_ctz(L2_CACHE_BLOCK_SIZE));
            l2_writeback_line(line, victim_addr);
        }
        l2_fill_line(line, block_addr, tag);
    }
    line->data[offset] = data;
    line->dirty = true;
}

/* L1 Cache Operations */
static L1_CacheLine *l1_select_line(L1_CacheSet *set, uint32_t tag) {
    L1_CacheLine *invalid = NULL;
    int i;
    for(i = 0; i < L1_CACHE_WAYS; i ++) {
        L1_CacheLine *line = &set->lines[i];
        if(line->valid && line->tag == tag) {
            return line;
        }
        if(!line->valid && invalid == NULL) {
            invalid = line;
        }
    }
    if(invalid) {
        return invalid;
    }
    uint32_t victim = next_rand() % L1_CACHE_WAYS;
    return &set->lines[victim];
}

static L1_CacheLine *l1_find_line(L1_CacheSet *set, uint32_t tag) {
    int i;
    for(i = 0; i < L1_CACHE_WAYS; i ++) {
        L1_CacheLine *line = &set->lines[i];
        if(line->valid && line->tag == tag) {
            return line;
        }
    }
    return NULL;
}

static void l1_fill_line(L1_CacheLine *line, hwaddr_t block_addr, uint32_t tag) {
    uint32_t i;
    for(i = 0; i < L1_CACHE_BLOCK_SIZE; i ++) {
        line->data[i] = l2_read_byte(block_addr + i);
    }
    line->tag = tag;
    line->valid = true;
}

static uint8_t l1_read_byte(hwaddr_t addr) {
    uint32_t set_idx = l1_addr_set(addr);
    uint32_t tag = l1_addr_tag(addr);
    uint32_t offset = l1_addr_offset(addr);

    L1_CacheSet *set = &l1_cache[set_idx];
    L1_CacheLine *line = l1_find_line(set, tag);
    if(line == NULL) {
        hwaddr_t block_addr = addr & ~L1_CACHE_BLOCK_MASK;
        line = l1_select_line(set, tag);
        l1_fill_line(line, block_addr, tag);
    }
    return line->data[offset];
}

static void l1_update_cache_byte(hwaddr_t addr, uint8_t data) {
    uint32_t set_idx = l1_addr_set(addr);
    uint32_t tag = l1_addr_tag(addr);
    uint32_t offset = l1_addr_offset(addr);

    L1_CacheSet *set = &l1_cache[set_idx];
    L1_CacheLine *line = l1_find_line(set, tag);
    if(line != NULL) {
        line->data[offset] = data;
    }
}

void init_cache(void) {
    int i, j;
    for(i = 0; i < L1_CACHE_SET_COUNT; i ++) {
        for(j = 0; j < L1_CACHE_WAYS; j ++) {
            l1_cache[i].lines[j].valid = false;
        }
    }
    for(i = 0; i < L2_CACHE_SET_COUNT; i ++) {
        for(j = 0; j < L2_CACHE_WAYS; j ++) {
            l2_cache[i].lines[j].valid = false;
            l2_cache[i].lines[j].dirty = false;
        }
    }
    rand_state = 1;
}

uint32_t cache_read(hwaddr_t addr, size_t len) {
#ifdef DEBUG
    assert(len == 1 || len == 2 || len == 4);
#endif
    uint32_t data = 0;
    size_t i;
    for(i = 0; i < len; i ++) {
        uint8_t byte = l1_read_byte(addr + i);
        data |= (uint32_t)byte << (i << 3);
    }
    return data;
}

void cache_write(hwaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
    assert(len == 1 || len == 2 || len == 4);
#endif
    size_t i;
    for(i = 0; i < len; i ++) {
        uint8_t byte = (data >> (i << 3)) & 0xff;
        l1_update_cache_byte(addr + i, byte);
        l2_write_byte(addr + i, byte);
    }
}
