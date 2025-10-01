#include "memory/cache.h"
#include "memory/memory.h"

uint32_t dram_read(hwaddr_t addr, size_t len);
void dram_write(hwaddr_t addr, size_t len, uint32_t data);

#define CACHE_SIZE (64 * 1024)
#define CACHE_BLOCK_SIZE 64
#define CACHE_WAYS 8

#define CACHE_BLOCK_MASK (CACHE_BLOCK_SIZE - 1)
#define CACHE_LINE_COUNT (CACHE_SIZE / CACHE_BLOCK_SIZE)
#define CACHE_SET_COUNT (CACHE_LINE_COUNT / CACHE_WAYS)

typedef struct {
    uint8_t data[CACHE_BLOCK_SIZE];
    uint32_t tag;
    bool valid;
} CacheLine;

typedef struct {
    CacheLine lines[CACHE_WAYS];
} CacheSet;

static CacheSet cache[CACHE_SET_COUNT];
static uint32_t rand_state = 1;

static inline uint32_t next_rand(void) {
    rand_state = rand_state * 1103515245 + 12345;
    return rand_state;
}

static inline uint32_t addr_tag(hwaddr_t addr) {
    return addr >> (__builtin_ctz(CACHE_BLOCK_SIZE) + __builtin_ctz(CACHE_SET_COUNT));
}

static inline uint32_t addr_set(hwaddr_t addr) {
    return (addr >> __builtin_ctz(CACHE_BLOCK_SIZE)) & (CACHE_SET_COUNT - 1);
}

static inline uint32_t addr_offset(hwaddr_t addr) {
    return addr & CACHE_BLOCK_MASK;
}

static CacheLine *select_line(CacheSet *set, uint32_t tag) {
    CacheLine *invalid = NULL;
    int i;
    for(i = 0; i < CACHE_WAYS; i ++) {
        CacheLine *line = &set->lines[i];
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
    uint32_t victim = next_rand() % CACHE_WAYS;
    return &set->lines[victim];
}

static CacheLine *find_line(CacheSet *set, uint32_t tag) {
    int i;
    for(i = 0; i < CACHE_WAYS; i ++) {
        CacheLine *line = &set->lines[i];
        if(line->valid && line->tag == tag) {
            return line;
        }
    }
    return NULL;
}

static void fill_line(CacheLine *line, hwaddr_t block_addr, uint32_t tag) {
    uint32_t i;
    for(i = 0; i < CACHE_BLOCK_SIZE; i += 4) {
        uint32_t val = dram_read(block_addr + i, 4);
        memcpy(line->data + i, &val, 4);
    }
    line->tag = tag;
    line->valid = true;
}

static uint8_t read_byte(hwaddr_t addr) {
    uint32_t set_idx = addr_set(addr);
    uint32_t tag = addr_tag(addr);
    uint32_t offset = addr_offset(addr);

    CacheSet *set = &cache[set_idx];
    CacheLine *line = find_line(set, tag);
    if(line == NULL) {
        hwaddr_t block_addr = addr & ~CACHE_BLOCK_MASK;
        line = select_line(set, tag);
        fill_line(line, block_addr, tag);
    }
    return line->data[offset];
}

static void update_cache_byte(hwaddr_t addr, uint8_t data) {
    uint32_t set_idx = addr_set(addr);
    uint32_t tag = addr_tag(addr);
    uint32_t offset = addr_offset(addr);

    CacheSet *set = &cache[set_idx];
    CacheLine *line = find_line(set, tag);
    if(line != NULL) {
        line->data[offset] = data;
    }
}

void init_cache(void) {
    int i, j;
    for(i = 0; i < CACHE_SET_COUNT; i ++) {
        for(j = 0; j < CACHE_WAYS; j ++) {
            cache[i].lines[j].valid = false;
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
        uint8_t byte = read_byte(addr + i);
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
        update_cache_byte(addr + i, byte);
    }
    dram_write(addr, len, data);
}
