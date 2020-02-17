// See LICENSE for license details.
// See LICENSE_CHERI for license details.

#ifndef _RISCV_MMU_H
#define _RISCV_MMU_H

#include "decode.h"
#include "trap.h"
#include "common.h"
#include "config.h"
#include "simif.h"
#include "processor.h"
#include "extension.h"
#include "memtracer.h"
#include "cachesim.h"
#include "byteorder.h"
#include <stdlib.h>
#include <vector>

// virtual memory configuration
#define PGSHIFT 12
const reg_t PGSIZE = 1 << PGSHIFT;
const reg_t PGMASK = ~(PGSIZE-1);
#define MAX_PADDR_BITS 56 // imposed by Sv39 / Sv48

struct insn_fetch_t
{
  insn_func_t func;
  insn_t insn;
};

struct icache_entry_t {
  reg_t tag;
  struct icache_entry_t* next;
  insn_fetch_t data;
};

struct tlb_entry_t {
  char* host_offset;
  reg_t target_offset;
};

class trigger_matched_t
{
  public:
    trigger_matched_t(int index,
        trigger_operation_t operation, reg_t address, reg_t data) :
      index(index), operation(operation), address(address), data(data) {}

    int index;
    trigger_operation_t operation;
    reg_t address;
    reg_t data;
};

// this class implements a processor's port into the virtual memory system.
// an MMU and instruction cache are maintained for simulator performance.
class mmu_t
{
public:
  mmu_t(simif_t* sim, processor_t* proc);
  ~mmu_t();

  inline reg_t misaligned_load(reg_t addr, size_t size, reg_t *paddr)
  {
#ifdef RISCV_ENABLE_MISALIGNED
    reg_t res = 0;
    for (size_t i = 0; i < size; i++)
      res += (reg_t)load_uint8(addr + i, i == 0 ? paddr : NULL) << (i * 8);
    return res;
#else
    throw trap_load_address_misaligned(addr);
#endif
  }

  inline void misaligned_store(reg_t addr, reg_t data, size_t size, reg_t *paddr)
  {
#ifdef RISCV_ENABLE_MISALIGNED
    for (size_t i = 0; i < size; i++)
      store_uint8(addr + i, data >> (i * 8), i == 0 ? paddr : NULL);

    if (proc) {
      if (proc->rvfi_dii) {
        proc->rvfi_dii_output.rvfi_dii_mem_addr = addr;
        proc->rvfi_dii_output.rvfi_dii_mem_wdata = data;
        proc->rvfi_dii_output.rvfi_dii_mem_wmask = (1 << size) - 1;
      }
    }
#else
    throw trap_store_address_misaligned(addr);
#endif
  }

#ifndef RISCV_ENABLE_COMMITLOG
# define READ_MEM(addr, size) ({})
#else
# define READ_MEM(addr, size) \
  proc->state.log_mem_read.push_back(std::make_tuple(addr, 0, size));
#endif

  // template for functions that load an aligned value from memory

  #define misaligned_load_uint \
    data = misaligned_load(addr, sizeof(data), paddr); \
    goto success_load;

  #define load_func_impl(type, intval_expr, trace_mask_bytes, misaligned_code) \
    inline type##_t load_##type(reg_t addr, reg_t *paddr = NULL) { \
      type##_t data; \
      reg_t vpn = addr >> PGSHIFT; \
      size_t size = sizeof(type##_t); \
      if (unlikely(addr & (sizeof(type##_t)-1))) { \
        misaligned_code \
      }\
      if (likely(tlb_load_tag[vpn % TLB_ENTRIES] == vpn)) { \
        data = *(type##_t*)(tlb_data[vpn % TLB_ENTRIES].host_offset + addr); \
        if (paddr) \
          *paddr = tlb_data[vpn % TLB_ENTRIES].target_offset + addr; \
        goto success_load; \
      } \
      if (unlikely(tlb_load_tag[vpn % TLB_ENTRIES] == (vpn | TLB_CHECK_TRIGGERS))) { \
        data = *(type##_t*)(tlb_data[vpn % TLB_ENTRIES].host_offset + addr); \
        if (paddr) \
          *paddr = tlb_data[vpn % TLB_ENTRIES].target_offset + addr; \
        if (!matched_trigger) { \
          matched_trigger = trigger_exception(OPERATION_LOAD, addr, (intval_expr)); \
          if (matched_trigger) \
            throw *matched_trigger; \
        } \
        goto success_load; \
      } \
      load_slow_path(addr, sizeof(type##_t), (trace_mask_bytes), \
                     (uint8_t*)&data, paddr); \
      goto success_load; \
      success_load: \
      if (proc) { \
        READ_MEM(addr, size); \
        if (proc->rvfi_dii) { \
          proc->rvfi_dii_output.rvfi_dii_mem_addr = addr; \
          proc->rvfi_dii_output.rvfi_dii_mem_rdata = intval_expr; \
          proc->rvfi_dii_output.rvfi_dii_mem_rmask = (1 << (trace_mask_bytes)) - 1; \
        } \
      } \
      return from_le(data); \
    }

  #define load_func(type) \
    load_func_impl(type, data, sizeof(type##_t), misaligned_load_uint)

  // load value from memory at aligned address; zero extend to register width
  load_func(uint8)
  load_func(uint16)
  load_func(uint32)
  load_func(uint64)

  // load value from memory at aligned address; sign extend to register width
  load_func(int8)
  load_func(int16)
  load_func(int32)
  load_func(int64)

#ifdef ENABLE_CHERI
  #define misaligned_load_cap throw trap_load_address_misaligned(addr);
  load_func_impl(cheri_reg_inmem, data.cursor, 8, misaligned_load_cap)
  #undef misaligned_load_cap
#endif

  #undef load_func
  #undef load_func_impl
  #undef misaligned_load_uint

#ifndef RISCV_ENABLE_COMMITLOG
# define WRITE_MEM(addr, value, size) ({})
#else
# define WRITE_MEM(addr, val, size) \
  proc->state.log_mem_write.push_back(std::make_tuple(addr, val, size));
#endif

  // template for functions that store an aligned value to memory

  #define misaligned_store_uint \
    return misaligned_store(addr, val, sizeof(val), paddr);

  #define store_func_impl(type, intval_expr, trace_mask_bytes, misaligned_code) \
    void store_##type(reg_t addr, type##_t val, reg_t *paddr = NULL) { \
      reg_t vpn = addr >> PGSHIFT; \
      size_t size = sizeof(type##_t); \
      if (unlikely(addr & (sizeof(type##_t)-1))) { \
        misaligned_code \
      } \
      if (likely(tlb_store_tag[vpn % TLB_ENTRIES] == vpn)) { \
        if (proc) WRITE_MEM(addr, val, size); \
        *(type##_t*)(tlb_data[vpn % TLB_ENTRIES].host_offset + addr) = to_le(val); \
        if (paddr) \
          *paddr = tlb_data[vpn % TLB_ENTRIES].target_offset + addr; \
      } \
      else if (unlikely(tlb_store_tag[vpn % TLB_ENTRIES] == (vpn | TLB_CHECK_TRIGGERS))) { \
        if (!matched_trigger) { \
          matched_trigger = trigger_exception(OPERATION_STORE, addr, (intval_expr)); \
          if (matched_trigger) \
            throw *matched_trigger; \
        } \
        if (proc) WRITE_MEM(addr, val, size); \
        *(type##_t*)(tlb_data[vpn % TLB_ENTRIES].host_offset + addr) = to_le(val); \
        if (paddr) \
          *paddr = tlb_data[vpn % TLB_ENTRIES].target_offset + addr; \
      } \
      else { \
        if (proc) WRITE_MEM(addr, val, size); \
        store_slow_path(addr, sizeof(type##_t), (trace_mask_bytes), \
                        (const uint8_t*)&val, paddr); \
      } \
      if (proc) { \
        if (proc->rvfi_dii) { \
          proc->rvfi_dii_output.rvfi_dii_mem_addr = addr; \
          proc->rvfi_dii_output.rvfi_dii_mem_wdata = intval_expr; \
          proc->rvfi_dii_output.rvfi_dii_mem_wmask = (1 << (trace_mask_bytes)) - 1; \
        } \
      } \
    }

  #define store_func(type) \
    store_func_impl(type, val, sizeof(val), misaligned_store_uint)

  // template for functions that perform an atomic memory operation
  #define amo_func(type) \
    template<typename op> \
    type##_t amo_##type(reg_t addr, op f, reg_t *paddr = NULL) { \
      if (addr & (sizeof(type##_t)-1)) \
        throw trap_store_address_misaligned(addr); \
      try { \
        auto lhs = load_##type(addr, paddr); \
        store_##type(addr, f(lhs)); \
        return lhs; \
      } catch (trap_load_page_fault& t) { \
        /* AMO faults should be reported as store faults */ \
        throw trap_store_page_fault(t.get_tval()); \
      } catch (trap_load_access_fault& t) { \
        /* AMO faults should be reported as store faults */ \
        throw trap_store_access_fault(t.get_tval()); \
      } \
    }

  void store_float128(reg_t addr, float128_t val, reg_t *paddr = NULL)
  {
#ifndef RISCV_ENABLE_MISALIGNED
    if (unlikely(addr & (sizeof(float128_t)-1)))
      throw trap_store_address_misaligned(addr);
#endif
    store_uint64(addr, val.v[0], paddr);
    store_uint64(addr + 8, val.v[1]);
  }

  float128_t load_float128(reg_t addr, reg_t *paddr = NULL)
  {
#ifndef RISCV_ENABLE_MISALIGNED
    if (unlikely(addr & (sizeof(float128_t)-1)))
      throw trap_load_address_misaligned(addr);
#endif
    return (float128_t){load_uint64(addr, paddr), load_uint64(addr + 8)};
  }

  // store value to memory at aligned address
  store_func(uint8)
  store_func(uint16)
  store_func(uint32)
  store_func(uint64)

#ifdef ENABLE_CHERI
  #define misaligned_store_cap throw trap_store_address_misaligned(addr);
  store_func_impl(cheri_reg_inmem, val.cursor, 8, misaligned_store_cap)
  #undef misaligned_store_cap
#endif

  #undef store_func
  #undef store_func_impl
  #undef misaligned_store_uint

  // perform an atomic memory operation at an aligned address
#ifdef ENABLE_CHERI
  amo_func(uint8)
  amo_func(uint16)
#endif
  amo_func(uint32)
  amo_func(uint64)

#ifdef ENABLE_CHERI
  amo_func(cheri_reg_inmem)
#endif

  #undef amo_func

  inline void yield_load_reservation()
  {
    load_reservation_address = (reg_t)-1;
  }

  inline void acquire_load_reservation(reg_t vaddr)
  {
    reg_t paddr = translate(vaddr, 1, LOAD);
    if (auto host_addr = sim->addr_to_mem(paddr))
      load_reservation_address = refill_tlb(vaddr, paddr, host_addr, LOAD).target_offset + vaddr;
    else
      throw trap_load_access_fault(vaddr); // disallow LR to I/O space
  }

  inline bool check_load_reservation(reg_t vaddr)
  {
    reg_t paddr = translate(vaddr, 1, STORE);
    if (auto host_addr = sim->addr_to_mem(paddr))
      return load_reservation_address == refill_tlb(vaddr, paddr, host_addr, STORE).target_offset + vaddr;
    else
      throw trap_store_access_fault(vaddr); // disallow SC to I/O space
  }

#ifdef ENABLE_CHERI
  /* Currently CHERI need to know paddr to set tagged bits */
  reg_t translate(reg_t addr, reg_t len, access_type type);
#endif /* ENABLE_CHERI */
  static const reg_t ICACHE_ENTRIES = 1024;

  inline size_t icache_index(reg_t addr)
  {
    return (addr / PC_ALIGN) % ICACHE_ENTRIES;
  }

  inline icache_entry_t* refill_icache(reg_t addr, icache_entry_t* entry)
  {
    auto *ext = proc->get_extension();
    if (ext) ext->check_ifetch_granule(addr, addr);

    auto tlb_entry = translate_insn_addr(addr);
    insn_bits_t insn = from_le(*(uint16_t*)(tlb_entry.host_offset + addr));
    int length = insn_length(insn);

    if (likely(length == 4)) {
      if (ext) ext->check_ifetch_granule(addr, addr + 2);
      insn |= (insn_bits_t)from_le(*(const int16_t*)translate_insn_addr_to_host(addr + 2)) << 16;
    } else if (length == 2) {
      insn = (int16_t)insn;
    } else if (length == 6) {
      insn |= (insn_bits_t)from_le(*(const uint16_t*)translate_insn_addr_to_host(addr + 2)) << 16;
      if (ext) ext->check_ifetch_granule(addr, addr + 2);
      insn |= (insn_bits_t)from_le(*(const int16_t*)translate_insn_addr_to_host(addr + 4)) << 32;
      if (ext) ext->check_ifetch_granule(addr, addr + 4);
    } else {
      static_assert(sizeof(insn_bits_t) == 8, "insn_bits_t must be uint64_t");
      insn |= (insn_bits_t)from_le(*(const uint16_t*)translate_insn_addr_to_host(addr + 2)) << 16;
      if (ext) ext->check_ifetch_granule(addr, addr + 2);
      insn |= (insn_bits_t)from_le(*(const uint16_t*)translate_insn_addr_to_host(addr + 4)) << 32;
      if (ext) ext->check_ifetch_granule(addr, addr + 4);
      insn |= (insn_bits_t)from_le(*(const int16_t*)translate_insn_addr_to_host(addr + 6)) << 48;
      if (ext) ext->check_ifetch_granule(addr, addr + 6);
    }

    insn_fetch_t fetch = {proc->decode_insn(insn), insn};
    entry->tag = addr;
    entry->next = &icache[icache_index(addr + length)];
    entry->data = fetch;

    reg_t paddr = tlb_entry.target_offset + addr;;
    if (tracer.interested_in_range(paddr, paddr + 1, FETCH)) {
      entry->tag = -1;
      tracer.trace(paddr, length, FETCH);
    }
    return entry;
  }

  inline icache_entry_t* access_icache(reg_t addr)
  {
    icache_entry_t* entry = &icache[icache_index(addr)];
    if (likely(entry->tag == addr)) {
      if (auto *ext = proc->get_extension()) {
        ext->check_ifetch_granule(addr, addr);
        for (int i = 2; i < entry->data.insn.length(); i += 2)
          ext->check_ifetch_granule(addr, addr + i);
      }
      return entry;
    }
    return refill_icache(addr, entry);
  }

  inline insn_fetch_t load_insn(reg_t addr)
  {
    icache_entry_t entry;
    return refill_icache(addr, &entry)->data;
  }

  void flush_tlb();
  void flush_icache();

  void register_memtracer(memtracer_t*);
  void register_l2cache(cache_sim_t* l2) {l2cache = l2;}
  cache_sim_t *get_l2cache() {return l2cache;}

  memtracer_list_t *get_tracer() {return &tracer;};

  int is_dirty_enabled()
  {
#ifdef RISCV_ENABLE_DIRTY
    return 1;
#else
    return 0;
#endif
  }

  int is_misaligned_enabled()
  {
#ifdef RISCV_ENABLE_MISALIGNED
    return 1;
#else
    return 0;
#endif
  }

private:
  simif_t* sim;
  processor_t* proc;
  memtracer_list_t tracer;
  reg_t load_reservation_address;
  cache_sim_t *l2cache;
  uint16_t fetch_temp;

  // implement an instruction cache for simulator performance
  icache_entry_t icache[ICACHE_ENTRIES];

  // implement a TLB for simulator performance
  static const reg_t TLB_ENTRIES = 256;
  // If a TLB tag has TLB_CHECK_TRIGGERS set, then the MMU must check for a
  // trigger match before completing an access.
  static const reg_t TLB_CHECK_TRIGGERS = reg_t(1) << 63;
  tlb_entry_t tlb_data[TLB_ENTRIES];
  reg_t tlb_insn_tag[TLB_ENTRIES];
  reg_t tlb_load_tag[TLB_ENTRIES];
  reg_t tlb_store_tag[TLB_ENTRIES];

  // finish translation on a TLB miss and update the TLB
  tlb_entry_t refill_tlb(reg_t vaddr, reg_t paddr, char* host_addr, access_type type);
  const char* fill_from_mmio(reg_t vaddr, reg_t paddr);

  // perform a page table walk for a given VA; set referenced/dirty bits
  reg_t walk(reg_t addr, access_type type, reg_t prv);

  // handle uncommon cases: TLB misses, page faults, MMIO
  tlb_entry_t fetch_slow_path(reg_t addr);
  void load_slow_path(reg_t addr, reg_t len, reg_t trace_mask_bytes,
                      uint8_t* bytes, reg_t* paddr);
  void store_slow_path(reg_t addr, reg_t len, reg_t trace_mask_bytes,
                       const uint8_t* bytes, reg_t* paddr);
#ifndef ENABLE_CHERI
  reg_t translate(reg_t addr, reg_t len, access_type type);
#endif

  // ITLB lookup
  inline tlb_entry_t translate_insn_addr(reg_t addr) {
    reg_t vpn = addr >> PGSHIFT;
    if (likely(tlb_insn_tag[vpn % TLB_ENTRIES] == vpn))
      return tlb_data[vpn % TLB_ENTRIES];
    tlb_entry_t result;
    if (unlikely(tlb_insn_tag[vpn % TLB_ENTRIES] != (vpn | TLB_CHECK_TRIGGERS))) {
      result = fetch_slow_path(addr);
    } else {
      result = tlb_data[vpn % TLB_ENTRIES];
    }
    if (unlikely(tlb_insn_tag[vpn % TLB_ENTRIES] == (vpn | TLB_CHECK_TRIGGERS))) {
      uint16_t* ptr = (uint16_t*)(tlb_data[vpn % TLB_ENTRIES].host_offset + addr);
      int match = proc->trigger_match(OPERATION_EXECUTE, addr, from_le(*ptr));
      if (match >= 0) {
        throw trigger_matched_t(match, OPERATION_EXECUTE, addr, from_le(*ptr));
      }
    }
    return result;
  }

  inline const uint16_t* translate_insn_addr_to_host(reg_t addr) {
    return (uint16_t*)(translate_insn_addr(addr).host_offset + addr);
  }

  // Bypasses extension's check_ifetch_granule for use by a debugger!
  inline insn_bits_t get_insn(reg_t addr)
  {
    auto tlb_entry = translate_insn_addr(addr);
    insn_bits_t insn = *(uint16_t*)(tlb_entry.host_offset + addr);
    int length = insn_length(insn);

    if (likely(length == 4)) {
      insn |= (insn_bits_t)from_le(*(const int16_t*)translate_insn_addr_to_host(addr + 2)) << 16;
    } else if (length == 2) {
      insn = (int16_t)insn;
    } else if (length == 6) {
      insn |= (insn_bits_t)from_le(*(const uint16_t*)translate_insn_addr_to_host(addr + 2)) << 16;
      insn |= (insn_bits_t)from_le(*(const int16_t*)translate_insn_addr_to_host(addr + 4)) << 32;
    } else {
      static_assert(sizeof(insn_bits_t) == 8, "insn_bits_t must be uint64_t");
      insn |= (insn_bits_t)from_le(*(const uint16_t*)translate_insn_addr_to_host(addr + 2)) << 16;
      insn |= (insn_bits_t)from_le(*(const uint16_t*)translate_insn_addr_to_host(addr + 4)) << 32;
      insn |= (insn_bits_t)from_le(*(const int16_t*)translate_insn_addr_to_host(addr + 6)) << 48;
    }

    return insn;
  }

  inline trigger_matched_t *trigger_exception(trigger_operation_t operation,
      reg_t address, reg_t data)
  {
    if (!proc) {
      return NULL;
    }
    int match = proc->trigger_match(operation, address, data);
    if (match == -1)
      return NULL;
    if (proc->state.mcontrol[match].timing == 0) {
      throw trigger_matched_t(match, operation, address, data);
    }
    return new trigger_matched_t(match, operation, address, data);
  }

  reg_t pmp_homogeneous(reg_t addr, reg_t len);
  reg_t pmp_ok(reg_t addr, reg_t len, access_type type, reg_t mode);

  bool check_triggers_fetch;
  bool check_triggers_load;
  bool check_triggers_store;
  // The exception describing a matched trigger, or NULL.
  trigger_matched_t *matched_trigger;

  friend class processor_t;
  friend class sim_t;
};

struct vm_info {
  int levels;
  int idxbits;
  int ptesize;
  reg_t ptbase;
};

inline vm_info decode_vm_info(int xlen, reg_t prv, reg_t satp)
{
  if (prv == PRV_M) {
    return {0, 0, 0, 0};
  } else if (prv <= PRV_S && xlen == 32) {
    switch (get_field(satp, SATP32_MODE)) {
      case SATP_MODE_OFF: return {0, 0, 0, 0};
      case SATP_MODE_SV32: return {2, 10, 4, (satp & SATP32_PPN) << PGSHIFT};
      default: abort();
    }
  } else if (prv <= PRV_S && xlen == 64) {
    switch (get_field(satp, SATP64_MODE)) {
      case SATP_MODE_OFF: return {0, 0, 0, 0};
      case SATP_MODE_SV39: return {3, 9, 8, (satp & SATP64_PPN) << PGSHIFT};
      case SATP_MODE_SV48: return {4, 9, 8, (satp & SATP64_PPN) << PGSHIFT};
      case SATP_MODE_SV57: return {5, 9, 8, (satp & SATP64_PPN) << PGSHIFT};
      case SATP_MODE_SV64: return {6, 9, 8, (satp & SATP64_PPN) << PGSHIFT};
      default: abort();
    }
  } else {
    abort();
  }
}

#endif
