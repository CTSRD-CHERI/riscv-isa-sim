/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Hesham Almatary <Hesham.Almatary@cl.cam.ac.uk>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#ifdef ENABLE_CHERI
#ifndef _RISCV_CHERI_H
#define _RISCV_CHERI_H

#include "extension.h"
#include "cheri_trap.h"
#include "cheri-compressed-cap/cheri_compressed_cap.h"
#include "mmu.h"
#include "tags.h"

#define DEBUG 0
#define BIT(x) (1ull << (x))
#define MASK(n) ((BIT(n)-1ull))

#define CHERI (static_cast<cheri_t*>(p->get_extension()))
#define CHERI_STATE CHERI->state
#define NUM_CHERI_SCR_REGS 32

#define SCR CHERI->get_scr(insn.chs(), p)
#define PCC CHERI->get_scr(CHERI_SCR_PCC, p)
#define DDC CHERI_STATE.scrs_reg_file[CHERI_SCR_DDC]

#define SET_SCR(index, val) CHERI->set_scr(index, val, p)
#define GET_SCR(index) CHERI->get_scr(index, p)

#define NUM_CHERI_REGS 32

#ifdef CHERI_MERGED_RF
# define CD  STATE.XPR[insn.cd()]
# define CS1 STATE.XPR[insn.cs1()]
# define CS2 STATE.XPR[insn.cs2()]

# define WRITE_CD(val) ({ \
  cheri_reg_t wdata = (val); /* val may have side effects */ \
  STATE.XPR.write(insn.cd(), wdata); \
  if (p->rvfi_dii && (insn.cd() != 0)) { \
    p->rvfi_dii_output.rvfi_dii_rd_wdata = wdata.base + wdata.offset; \
    p->rvfi_dii_output.rvfi_dii_rd_addr = insn.cd(); \
  } \
  if(DEBUG) { \
    fprintf(stderr, "x%lu <- t:%u s:%u perms:0x%08x type:0x%016x offset:0x%016lx base:0x%016lx length:0x%1lx %016lx\n", insn.cd(), wdata.tag, wdata.sealed, wdata.perms, wdata.otype, wdata.offset, wdata.base, (uint64_t) (wdata.length >> 64), (uint64_t) wdata.length & UINT64_MAX); \
  } \
})
# define READ_CREG(reg) STATE.XPR[reg]
# define WRITE_CREG(reg, val) STATE.XPR.write(reg, val)
#else/* CHERI_MERGED_RF */
# define CD  CHERI_STATE.reg_file[insn.cd()]
# define CS1 CHERI_STATE.reg_file[insn.cs1()]
# define CS2 CHERI_STATE.reg_file[insn.cs2()]

# define WRITE_CD(val) CHERI_STATE.reg_file.write(insn.cd(), val)
# define READ_CREG(reg) CHERI_STATE.reg_file[reg]
# define WRITE_CREG(reg, val) CHERI_STATE.reg_file.write(reg, val)
#endif /* CHERI_MERGED_RF */

#ifdef ENABLE_CHERI128
# define CHERI_CAPSIZE_BYTES 16
#else
# define CHERI_CAPSIZE_BYTES 32
#endif /* ENABLE_CHERI128 */

extern const char *cheri_reg_names[32];

/* 256-bit Caps register format *
 * -------------------------------------------------------------------------
 * | length | base | offset | uperms | perms | S | reserved | otype | Tag  |
 * -------------------------------------------------------------------------
 * |  64    |  64  |   64   |   16   |  15   | 1 |   7     |   24   |  1   |
 * -------------------------------------------------------------------------
 */

struct cheri_state {
#ifndef CHERI_MERGED_RF
  regfile_t<cheri_reg_t, NUM_CHERI_REGS, true> reg_file;
#endif

  regfile_t<cheri_reg_t, NUM_CHERI_SCR_REGS, false> scrs_reg_file;
};

typedef struct cheri_state cheri_state_t;

void convertCheriReg(cap_register_t *destination, const cheri_reg_t *source);

void retrieveCheriReg(cheri_reg_t *destination, const cap_register_t *source);

bool cheri_is_representable(uint32_t sealed, uint64_t base, cheri_length_t length, uint64_t offset);

class cheri_t : public extension_t {
 public:

  cheri_t() {
  }

  ~cheri_t() {
  }

  /* Override extension functions */
  void reset();

  void register_insn(insn_desc_t desc);

  reg_t from_arch_pc(reg_t pc) {
    cheri_reg_t pcc = state.scrs_reg_file[CHERI_SCR_PCC];
    return pc + pcc.base;
  }

  reg_t to_arch_pc(reg_t pc) {
    cheri_reg_t pcc = state.scrs_reg_file[CHERI_SCR_PCC];
    return pc - pcc.base;
  }

  void check_ifetch_granule(reg_t start_addr, reg_t addr) {
    cheri_reg_t auth = state.scrs_reg_file[CHERI_SCR_PCC];
    reg_t authidx = 0x20 | CHERI_SCR_PCC;
    /* We can skip everything but bounds for granules other than the first. */
    if (start_addr == addr) {
      reg_t offset = addr - auth.base - auth.offset;
      memop_to_addr(auth, authidx, offset, 2, /*load=*/false, /*store=*/false,
                    /*execute=*/true, /*cap_op=*/false, /*store_local=*/false);
    } else {
      check_in_bounds(auth, authidx, addr, 2);
    }
  }

  /* Allocate tag_bits memory to cover all memory */
  void create_tagged_memory(size_t memsz);
  bool get_tag(reg_t addr);
  bool get_tag_translated(reg_t paddr);
  void set_tag(reg_t addr, bool val);
  void set_tag_translated(reg_t paddr, bool val);

  uint32_t get_clen() {
    return clen;
  };
  reg_t get_ccsr() {
    return ccsr;
  };
  void set_ccsr(reg_t c) {
    ccsr = c;
  };

  bool get_mode() {
    return !!CHERI_STATE.scrs_reg_file[CHERI_SCR_PCC].flags;
  };

  void raise_trap(reg_t trap_code, reg_t trap_reg) {
    set_ccsr((trap_code << 8) | trap_reg);
    throw trap_cheri_trap();
  };

  std::vector<insn_desc_t> get_instructions();
  std::vector<disasm_insn_t*> get_disasms(int xlen);

  cheri_state_t state;

  void set_scr(int index, cheri_reg_t val, processor_t* proc);

  cheri_reg_t get_scr(int index, processor_t* proc);

  void check_in_bounds(cheri_reg_t auth, reg_t authidx, reg_t addr,
                       reg_t len) {
    if (addr < auth.base || addr + len > auth.base + auth.length ||
        addr + len < addr) {
      raise_trap(CAUSE_CHERI_LENGTH_FAULT, authidx);
    }
  }

  /*
   * Trap if the given capability is not valid for the given memory
   * operation. Both load and store are allowed for AMOs, but load faults
   * take precendence over store faults.
   */
  reg_t memop_to_addr(cheri_reg_t auth, reg_t authidx, reg_t offset, reg_t len,
                      bool load, bool store, bool execute, bool cap_op,
                      bool store_local) {
    // Check we have at least one, and execute is exclusive.
    assert((load || store) != execute);
    // Check that store_local only occurs when we are doing a store cap.
    assert((cap_op && store) || !store_local);
    // Check that we never execute capabilities.
    assert(!(execute && cap_op));

    reg_t addr = auth.base + auth.offset + offset;
    if (!auth.tag) {
      raise_trap(CAUSE_CHERI_TAG_FAULT, authidx);
    } else if (auth.sealed) {
      raise_trap(CAUSE_CHERI_SEAL_FAULT, authidx);
    } else if (load && !(auth.perms & BIT(CHERI_PERMIT_LOAD))) {
      raise_trap(CAUSE_CHERI_PERMIT_LOAD_FAULT, authidx);
    } else if (load && cap_op && !(auth.perms & BIT(CHERI_PERMIT_LOAD_CAPABILITY))) {
      raise_trap(CAUSE_CHERI_PERMIT_LOAD_CAPABILITY_FAULT, authidx);
    } else if (store && !(auth.perms & BIT(CHERI_PERMIT_STORE))) {
      raise_trap(CAUSE_CHERI_PERMIT_STORE_FAULT, authidx);
    } else if (store && store_local &&
               !(auth.perms & BIT(CHERI_PERMIT_STORE_LOCAL_CAPABILITY))) {
      raise_trap(CAUSE_CHERI_PERMIT_STORE_LOCAL_CAPABILITY_FAULT, authidx);
    }
    check_in_bounds(auth, authidx, addr, len);
    return addr;
  }

  #define load_func(type) \
    inline type##_t cap_load_##type(cheri_reg_t auth, reg_t authidx, \
                                    int64_t offset) { \
      reg_t addr = \
        memop_to_addr(auth, authidx, offset, sizeof(type##_t), \
                      /*load=*/true, /*store=*/false, /*execute=*/false, \
                      /*cap_op=*/false, /*store_local=*/false); \
      return MMU.load_##type(addr); \
    } \
    \
    inline type##_t ddc_load_##type(reg_t addr) { \
      cheri_reg_t auth = state.scrs_reg_file[CHERI_SCR_DDC]; \
      reg_t authidx = 0x20 | CHERI_SCR_DDC; \
      return cap_load_##type(auth, authidx, addr); \
    }

  load_func(uint8)
  load_func(uint16)
  load_func(uint32)
  load_func(uint64)
  load_func(int8)
  load_func(int16)
  load_func(int32)
  load_func(int64)
  #undef load_func

  inline cheri_reg_t cap_load_cap(cheri_reg_t auth, reg_t authidx,
                                  int64_t offset) {
    reg_t addr =
      memop_to_addr(auth, authidx, offset, sizeof(cheri_reg_t), /*load=*/true,
                    /*store=*/false, /*execute=*/false, /*cap_op=*/true,
                    /*store_local=*/false);
    reg_t paddr;
    cheri_reg_inmem_t inmem = MMU.load_cheri_reg_inmem(addr, &paddr);
#ifdef ENABLE_CHERI128
    cheri_reg_t ret;
    cap_register_t converted;
    decompress_128cap(inmem.pesbt, inmem.cursor, &converted);
    retrieveCheriReg(&ret, &converted);
#else
    cheri_reg_t ret = inmem;
#endif
    ret.tag = (auth.perms & BIT(CHERI_PERMIT_LOAD_CAPABILITY)) &&
              get_tag_translated(paddr);
    return ret;
  }

  inline cheri_reg_t ddc_load_cap(reg_t addr) {
    cheri_reg_t auth = state.scrs_reg_file[CHERI_SCR_DDC];
    reg_t authidx = 0x20 | CHERI_SCR_DDC;
    return cap_load_cap(auth, authidx, addr);
  }

  #define store_func(type) \
    inline void cap_store_##type(cheri_reg_t auth, reg_t authidx, \
                                 int64_t offset, type##_t val) { \
      reg_t addr = \
        memop_to_addr(auth, authidx, offset, sizeof(type##_t), \
                      /*load=*/false, /*store=*/true, /*execute=*/false, \
                      /*cap_op=*/false, /*store_local=*/false); \
      reg_t paddr; \
      MMU.store_##type(addr, val, &paddr); \
      set_tag_translated(paddr, false); \
    } \
    \
    inline void ddc_store_##type(reg_t addr, type##_t val) { \
      cheri_reg_t auth = state.scrs_reg_file[CHERI_SCR_DDC]; \
      reg_t authidx = 0x20 | CHERI_SCR_DDC; \
      cap_store_##type(auth, authidx, addr, val); \
    }

  store_func(uint8)
  store_func(uint16)
  store_func(uint32)
  store_func(uint64)
  #undef store_func

  inline void cap_store_cap(cheri_reg_t auth, reg_t authidx, int64_t offset,
                            cheri_reg_t val) {
    bool store_local = val.tag && !(val.perms & BIT(CHERI_PERMIT_GLOBAL));
    reg_t addr =
      memop_to_addr(auth, authidx, offset, sizeof(cheri_reg_inmem_t),
                    /*load=*/false, /*store=*/true, /*execute=*/false,
                    /*cap_op=*/true, store_local);
    cheri_reg_inmem_t inmem;
#ifdef ENABLE_CHERI128
    cap_register_t converted;
    convertCheriReg(&converted, &val);
    inmem.pesbt = compress_128cap(&converted);
    inmem.cursor = converted.cr_base + converted.cr_offset;
#else
    inmem = val;
#endif
    reg_t paddr;
    MMU.store_cheri_reg_inmem(addr, inmem, &paddr);
    set_tag_translated(paddr, val.tag);
  }

  inline void ddc_store_cap(reg_t addr, cheri_reg_t val) {
    cheri_reg_t auth = state.scrs_reg_file[CHERI_SCR_DDC];
    reg_t authidx = 0x20 | CHERI_SCR_DDC;
    cap_store_cap(auth, authidx, addr, val);
  }

 private:
  /* FIXME: For now assume DRAM size is 2GiB, the default for Spike */
  tags_t<bool, (BIT(31) / sizeof(cheri_reg_inmem_t))> mem_tags;
  uint32_t clen = 0;
  reg_t ccsr = 0;
  std::vector<insn_desc_t> instructions;
};

#endif /* _RISCV_CHERI_H */
#endif /* ENABLE_CHERI */
