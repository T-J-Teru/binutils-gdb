#ifndef MRK3_MEM_MAP_H
#define MRK3_MEM_MAP_H

#define MRK3_MEM_TYPE_MASK 0x01000000
#define MRK3_MEM_TYPE_DATA 0x00000000
#define MRK3_MEM_TYPE_CODE 0x01000000

#define MRK3_MEM_SPACE_MASK 0xF0000000
#define MRK3_MEM_SPACE_BIT_OFFSET 28
#define MRK3_MEM_SPACE_MAP 0x00000000
#define MRK3_MEM_SPACE_SYS 0x10000000
#define MRK3_MEM_SPACE_APP1 0x20000000
#define MRK3_MEM_SPACE_APP2 0x30000000


// This requires basic types to be defined. So either you
// have defined them already somewhere else (e.g., you
// incuded defines.h) or they are defined here again.

static inline bool mrk3_is_code_addr(const uint32_t addr) {
    return (addr & MRK3_MEM_TYPE_MASK) == MRK3_MEM_TYPE_CODE;
}

static inline bool mrk3_is_data_addr(const uint32_t addr) {
    return (addr & MRK3_MEM_TYPE_MASK) == MRK3_MEM_TYPE_DATA;
}

static inline bool mrk3_is_map_addr(const uint32_t addr) {
    return (addr & MRK3_MEM_SPACE_MASK) == MRK3_MEM_SPACE_MAP;
}

static inline bool mrk3_is_sys_addr(const uint32_t addr) {
    return (addr & MRK3_MEM_SPACE_MASK) == MRK3_MEM_SPACE_SYS;
}

static inline bool mrk3_is_app1_addr(const uint32_t addr) {
    return (addr & MRK3_MEM_SPACE_MASK) == MRK3_MEM_SPACE_APP1;
}

static inline bool mrk3_is_app2_addr(const uint32_t addr) {
    return (addr & MRK3_MEM_SPACE_MASK) == MRK3_MEM_SPACE_APP2;
}

static inline uint32_t mrk3_to_data_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_TYPE_MASK)) | MRK3_MEM_TYPE_DATA;
}

static inline uint32_t mrk3_to_code_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_TYPE_MASK)) | MRK3_MEM_TYPE_CODE;
}

static inline uint32_t mrk3_to_map_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_SPACE_MASK)) | MRK3_MEM_SPACE_MAP;
}

static inline uint32_t mrk3_to_sys_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_SPACE_MASK)) | MRK3_MEM_SPACE_SYS;
}

static inline uint32_t mrk3_to_app1_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_SPACE_MASK)) | MRK3_MEM_SPACE_APP1;
}

static inline uint32_t mrk3_to_app2_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_SPACE_MASK)) | MRK3_MEM_SPACE_APP2;
}

static inline uint32_t mrk3_to_base_addr(const uint32_t addr) {
    return (addr & (~MRK3_MEM_TYPE_MASK) & (~MRK3_MEM_SPACE_MASK));
}

static inline uint32_t mrk3_remove_base_addr(const uint32_t addr) {
  return (addr & (MRK3_MEM_TYPE_MASK | MRK3_MEM_SPACE_MASK));
}

static inline uint8_t mrk3_mem_space_index(const uint32_t addr) {
  return (uint8_t) (((addr & MRK3_MEM_SPACE_MASK) >> MRK3_MEM_SPACE_BIT_OFFSET) - 1);
}

static inline uint32_t mrk3_mem_space_from_mem_space_index(const uint8_t mem_space_index) {
  return ((mem_space_index + 1) << MRK3_MEM_SPACE_BIT_OFFSET);
}

static inline uint32_t mrk3_to_mem_space(const uint32_t addr, const uint32_t mem_space) {
  return (addr & (~MRK3_MEM_SPACE_MASK)) | mem_space;
}

static inline uint32_t mrk3_to_mem_space_index(const uint32_t addr, const uint8_t mem_space_index) {
  return (addr & (~MRK3_MEM_SPACE_MASK)) | mrk3_mem_space_from_mem_space_index(mem_space_index);
}

// Convert a program counter to an address in the simulator
// memory (GDB view).
static inline uint32_t mrk3_pc_to_address(const uint32_t pc) {
	// The pc addresses words, but the simulator addresses bytes.
	// Therefore, the rom address must be multiplied by two.
	// The pc might also contain mem space information. Strip it
	// before multiplying.
	uint16_t base_pc = (uint16_t) pc;
	return mrk3_to_code_addr(mrk3_remove_base_addr(pc) | (base_pc << 1));
}


#endif // MRK3_MEM_MAP_H
