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
#include <inttypes.h>
#ifdef ENABLE_CHERI
#ifndef _RISCV_CHERI_H
#define _RISCV_CHERI_H

#include "extension.h"
#include "cheri_trap.h"
#include "cheri_types.h"
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

#define FMT32_HEX "0x%08" PRIx32
#define FMT64_HEX "0x%016" PRIx64

#ifdef CHERI_MERGED_RF
# define READ_CREG(reg) STATE.XPR[reg]
# define WRITE_CREG(reg, val) ({ \
  cheri_reg_t wdata = (val); /* val may have side effects */ \
  STATE.XPR.write((reg), wdata); \
  if (p->rvfi_dii && ((reg) != 0)) { \
    p->rvfi_dii_output.rvfi_dii_rd_wdata = wdata.cursor(); \
    p->rvfi_dii_output.rvfi_dii_rd_addr = (reg); \
  } \
  if(DEBUG) { \
    fprintf(stderr, "x%lu <- t:%u s:%u perms:" FMT32_HEX " type:" FMT32_HEX \
            " cursor:" FMT64_HEX " base:" FMT64_HEX " length:0x%1lx" FMT64_HEX \
            "\n", (long)(reg), wdata.tag, wdata.sealed(), wdata.perms, wdata.otype, \
            wdata.cursor(), wdata.base(), (long)(uint64_t) (wdata.length() >> 64), \
            (uint64_t) wdata.length() & UINT64_MAX); \
  } \
})
#else/* CHERI_MERGED_RF */
# define READ_CREG(reg) CHERI_STATE.reg_file[reg]
# define WRITE_CREG(reg, val) CHERI_STATE.reg_file.write(reg, val)
#endif /* CHERI_MERGED_RF */

# define CD  READ_CREG(insn.cd())
# define CS1 READ_CREG(insn.cs1())
# define CS2 READ_CREG(insn.cs2())

# define WRITE_CD(val) WRITE_CREG(insn.cd(), (val))

#define CHERI_CAPSIZE_BYTES 16

extern const char *cheri_reg_names[32];

struct cheri_state {
#ifndef CHERI_MERGED_RF
  regfile_t<cheri_reg_t, NUM_CHERI_REGS, true> reg_file;
#endif

  regfile_t<cheri_reg_t, NUM_CHERI_SCR_REGS, false> scrs_reg_file;
};

typedef struct cheri_state cheri_state_t;

class cheri_t : public extension_t {
 public:

  cheri_t() {
    printf("CHERI Extension is Enabled..\n");
  }

  ~cheri_t() {
  }

  const char* name() {
    return "cheri";
  }

  /* Override extension functions */
  void reset();

  void register_insn(insn_desc_t desc);

  reg_t from_arch_pc(reg_t pc) {
    cheri_reg_t pcc = state.scrs_reg_file[CHERI_SCR_PCC];
    return pc + pcc.base();
  }

  reg_t to_arch_pc(reg_t pc) {
    cheri_reg_t pcc = state.scrs_reg_file[CHERI_SCR_PCC];
    return pc - pcc.base();
  }

  void check_ifetch_granule(reg_t start_addr, reg_t addr) {
    cheri_reg_t auth = state.scrs_reg_file[CHERI_SCR_PCC];
    reg_t authidx = 0x20 | CHERI_SCR_PCC;
    /* We can skip everything but bounds for granules other than the first. */
    if (start_addr == addr) {
      reg_t offset = addr - auth.cursor();
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
    set_ccsr((trap_reg << 10) | (trap_code << 5));
    throw trap_cheri_trap((trap_reg << 5) | trap_code);
  };

  std::vector<insn_desc_t> get_instructions();
  std::vector<disasm_insn_t*> get_disasms(int xlen);

  cheri_state_t state;

  void set_scr(int index, cheri_reg_t val, processor_t* proc);

  cheri_reg_t get_scr(int index, processor_t* proc);

  void check_in_bounds(cheri_reg_t auth, reg_t authidx, reg_t addr,
                       reg_t len) {
    if (addr < auth.base() || addr + (cheri_length_t)len > auth.top()) {
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

    reg_t addr = auth.cursor() + offset;
    if (!auth.tag) {
      raise_trap(CAUSE_CHERI_TAG_FAULT, authidx);
    } else if (auth.sealed()) {
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
    } else if (execute && !(auth.perms & BIT(CHERI_PERMIT_EXECUTE))) {
      raise_trap(CAUSE_CHERI_PERMIT_EXECUTE_FAULT, authidx);
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
  load_func(float128)
  #undef load_func

  inline cheri_reg_t cap_load_cap(cheri_reg_t auth, reg_t authidx,
                                  int64_t offset) {
    reg_t addr =
      memop_to_addr(auth, authidx, offset, sizeof(cheri_reg_inmem_t),
                    /*load=*/true, /*store=*/false, /*execute=*/false,
                    /*cap_op=*/true, /*store_local=*/false);
    reg_t paddr;
    cheri_reg_inmem_t inmem = MMU.load_cheri_reg_inmem(addr, &paddr);
    bool tag = (auth.perms & BIT(CHERI_PERMIT_LOAD_CAPABILITY)) &&
               get_tag_translated(paddr);
    return cheri_reg_t(inmem, tag);
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
  store_func(float128)
  #undef store_func

  inline void cap_store_cap(cheri_reg_t auth, reg_t authidx, int64_t offset,
                            cheri_reg_t val) {
    bool store_local = val.tag && !(val.perms & BIT(CHERI_PERMIT_GLOBAL));
    reg_t addr =
      memop_to_addr(auth, authidx, offset, sizeof(cheri_reg_inmem_t),
                    /*load=*/false, /*store=*/true, /*execute=*/false,
                    /*cap_op=*/true, store_local);
    reg_t paddr;
    MMU.store_cheri_reg_inmem(addr, val.inmem(), &paddr);
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
