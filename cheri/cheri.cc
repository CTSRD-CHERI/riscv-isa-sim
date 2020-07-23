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
#include "cheri.h"
#include "trap.h"
#include "mmu.h"
#include "disasm.h"
#include <cstdlib>
#include <tgmath.h>
#include <stdio.h>
#include <stdarg.h>

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::to_string((int)insn.i_imm()) + '(' + xpr_name[insn.rs1()] + ')';
  }
} load_address;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::to_string((int)insn.s_imm()) + '(' + xpr_name[insn.rs1()] + ')';
  }
} store_address;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::string("(") + xpr_name[insn.rs1()] + ')';
  }
} amo_address;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::to_string((int)insn.i_imm()) + '(' + cheri_reg_names[insn.rs1()] + ')';
  }
} load_capability;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::to_string((int)insn.s_imm()) + '(' + cheri_reg_names[insn.rs1()] + ')';
  }
} store_capability;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::string("(") + cheri_reg_names[insn.rs1()] + ')';
  }
} amo_capability;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return xpr_name[insn.rd()];
  }
} xrd;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return xpr_name[insn.rs1()];
  }
} xrs1;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return xpr_name[insn.rs2()];
  }
} xrs2;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return cheri_reg_names[insn.rd()];
  }
} crd;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return cheri_reg_names[insn.rs1()];
  }
} crs1;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    if (insn.rs1() == 0) return "ddc";
    return cheri_reg_names[insn.rs1()];
  }
} crs1_ddc;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return cheri_reg_names[insn.rs2()];
  }
} crs2;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    if (insn.rs2() == 0) return "ddc";
    return cheri_reg_names[insn.rs2()];
  }
} crs2_ddc;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    switch (insn.chs())
    {
      #define DECLARE_CHERI_SCR(name, num) case num: return #name;
      #include "encoding.h"
      #undef DECLARE_CHERI_SCR
      default:
      {
        char buf[16];
        snprintf(buf, sizeof buf, "unknown_%03" PRIx64, insn.chs());
        return std::string(buf);
      }
    }
  }
} scr;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::to_string((int)insn.i_imm());
  }
} imm;

struct : public arg_t {
  std::string to_string(insn_t insn) const {
    return std::to_string(((unsigned)insn.i_imm()) & 0xFFF);
  }
} uimm;

template<class tag_t, unsigned int N>
constexpr unsigned int extract_tags_count(const tags_t<tag_t, N>&) { return N; }

bool cheri_t::get_tag(reg_t addr) {
  reg_t paddr = MMU.translate(addr, sizeof(cheri_reg_inmem_t), LOAD);
  return get_tag_translated(paddr);
}

bool cheri_t::get_tag_translated(reg_t paddr) {
  paddr -= DRAM_BASE;
  paddr >>= (int) log2(sizeof(cheri_reg_inmem_t));

  if (paddr >= extract_tags_count(mem_tags)) return false;
  return mem_tags.getTag(paddr);
}

void cheri_t::set_tag(reg_t addr, bool val) {
  reg_t paddr = MMU.translate(addr, sizeof(cheri_reg_inmem_t), STORE);
  set_tag_translated(paddr, val);
}

void cheri_t::set_tag_translated(reg_t paddr, bool val) {
  paddr -= DRAM_BASE;
  paddr >>= (int) log2(sizeof(cheri_reg_inmem_t));

  if (paddr >= extract_tags_count(mem_tags)) return;
  mem_tags.setTag(paddr, val);
}

#define CHERI_REGISTER_INSN(cheri, name, match, mask) \
  extern reg_t rv32cheri_##name(processor_t*, insn_t, reg_t); \
  extern reg_t rv64cheri_##name(processor_t*, insn_t, reg_t); \
  cheri->register_insn((insn_desc_t){match, mask, rv32cheri_##name, rv64cheri_##name});

void cheri_t::register_insn(insn_desc_t desc) {
  instructions.push_back(desc);
}

std::vector<insn_desc_t> cheri_t::get_instructions() {
  std::vector<insn_desc_t> insns;

#define DECLARE_INSN(name, match, mask) \
      insn_bits_t name##_match = (match), name##_mask = (mask);
#include "encoding.h"
#undef DECLARE_INSN

#define DEFINE_INSN(name) \
      CHERI_REGISTER_INSN(this, name, name##_match, name##_mask)
#include "cheri_insn_list.h"
#undef DEFINE_INSN

  return instructions;
}

std::vector<disasm_insn_t*> cheri_t::get_disasms(int xlen) {
  std::vector<disasm_insn_t*> insns;

  const uint32_t mask_rd = 0x1fUL << 7;
  const uint32_t match_rd_ra = 1UL << 7;
  const uint32_t mask_rs1 = 0x1fUL << 15;
  const uint32_t match_rs1_ra = 1UL << 15;
  const uint32_t mask_rs2 = 0x1fUL << 20;
  const uint32_t mask_imm = 0xfffUL << 20;
  const uint32_t match_imm_1 = 1UL << 20;
  const uint32_t mask_rvc_rs2 = 0x1fUL << 2;
  const uint32_t mask_rvc_imm = mask_rvc_rs2 | 0x1000UL;
  const uint32_t mask_nf = 0x7Ul << 29;

  #define DECLARE_INSN(code, match, mask) \
   const uint32_t match_##code = match; \
   const uint32_t mask_##code = mask;
  #include "encoding.h"
  #undef DECLARE_INSN

  #define DISASM_INSN(name, code, match, mask, ...) \
    insns.push_back(new disasm_insn_t(name, match_##code | (match), \
                    mask_##code | (mask), __VA_ARGS__));
  #define DEFINE_INSN(code, ...) \
    insns.push_back(new disasm_insn_t(#code, match_##code, mask_##code, \
                    __VA_ARGS__));

  /* Capability-Inspection Instructions */
  DEFINE_INSN(cgetperm, {&xrd, &crs1})
  DEFINE_INSN(cgettype, {&xrd, &crs1})
  DEFINE_INSN(cgetbase, {&xrd, &crs1})
  DEFINE_INSN(cgetlen, {&xrd, &crs1})
  DEFINE_INSN(cgettag, {&xrd, &crs1})
  DEFINE_INSN(cgetsealed, {&xrd, &crs1})
  DEFINE_INSN(cgetoffset, {&xrd, &crs1})
  DEFINE_INSN(cgetflags, {&xrd, &crs1})
  DEFINE_INSN(cgetaddr, {&xrd, &crs1})

  /* Capability-Modification Instructions */
  DEFINE_INSN(cseal, {&crd, &crs1})
  DEFINE_INSN(csealentry, {&crd, &crs1})
  DEFINE_INSN(cunseal, {&crd, &crs1})

  DEFINE_INSN(candperm, {&crd, &crs1, &xrs2})
  DEFINE_INSN(csetflags, {&crd, &crs1, &xrs2})
  DEFINE_INSN(csetoffset, {&crd, &crs1, &xrs2})
  DEFINE_INSN(csetaddr, {&crd, &crs1, &xrs2})
  DEFINE_INSN(cincoffset, {&crd, &crs1, &xrs2})
  DEFINE_INSN(cincoffsetimmediate, {&crd, &crs1, &imm})
  DEFINE_INSN(csetbounds, {&crd, &crs1, &xrs2})
  DEFINE_INSN(csetboundsexact, {&crd, &crs1, &xrs2})
  DEFINE_INSN(csetboundsimmediate, {&crd, &crs1, &uimm})

  DEFINE_INSN(ccleartag, {&crd, &xrs1})

  DEFINE_INSN(cbuildcap, {&crd, &crs1, &crs2})
  DEFINE_INSN(ccopytype, {&crd, &crs1, &crs2})
  DEFINE_INSN(ccseal, {&crd, &crs1, &crs2})

  /* Pointer-Arithmetic Instructions */
  DEFINE_INSN(ctoptr, {&xrd, &crs1, &crs2_ddc})
  DEFINE_INSN(cfromptr, {&crd, &crs1_ddc, &xrs2})

  DEFINE_INSN(csub, {&xrd, &crs1, &crs2})
  DEFINE_INSN(cmove, {&crd, &crs1})

  /* Control-Flow Instructions */
  DEFINE_INSN(cjalr, {&crd, &crs1})
  DISASM_INSN("cjr", cjalr, 0, mask_rd, {&crs1})
  DISASM_INSN("cret", cjalr, match_rs1_ra, mask_rs1, {&crs1})
  DEFINE_INSN(ccall, {&crs1, &crs2})

  /* Assertion Instructions */
  DEFINE_INSN(ctestsubset, {&xrd, &crs1, &crs2})

  /* Special Capability Register Access Instructions */
  DEFINE_INSN(cspecialrw, {&crd, &scr, &crs1})
  DISASM_INSN("cspecialr", cspecialrw, 0, mask_rs1, {&crd, &scr})
  DISASM_INSN("cspecialw", cspecialrw, 0, mask_rd, {&scr, &crs1})

  /* Fast Register-Clearing Instructions */
  /* TODO: Define in riscv-opcodes */

  /* Adjusting to Compressed Capability Precision Instructions */
  DEFINE_INSN(croundrepresentablelength, {&xrd, &xrs1})
  DEFINE_INSN(crepresentablealignmentmask, {&xrd, &xrs1})

  /* Memory Loads with Explicit Address Type Instructions */
  DEFINE_INSN(lb_ddc, {&xrd, &amo_address})
  DEFINE_INSN(lh_ddc, {&xrd, &amo_address})
  DEFINE_INSN(lw_ddc, {&xrd, &amo_address})
  if (xlen == 32) {
    DISASM_INSN("lc.ddc", ld_ddc, 0, 0, {&crd, &amo_address})
  } else {
    DEFINE_INSN(ld_ddc, {&xrd, &amo_address})
    DISASM_INSN("lc.ddc", lq_ddc, 0, 0, {&crd, &amo_address})
  }
  DEFINE_INSN(lbu_ddc, {&xrd, &amo_address})
  DEFINE_INSN(lhu_ddc, {&xrd, &amo_address})
  if (xlen == 64) {
    DEFINE_INSN(lwu_ddc, {&xrd, &amo_address})
    DEFINE_INSN(ldu_ddc, {&xrd, &amo_address})
  }

  DEFINE_INSN(lb_cap, {&xrd, &amo_capability})
  DEFINE_INSN(lh_cap, {&xrd, &amo_capability})
  DEFINE_INSN(lw_cap, {&xrd, &amo_capability})
  if (xlen == 32) {
    DISASM_INSN("lc.cap", ld_cap, 0, 0, {&crd, &amo_capability})
  } else {
    DEFINE_INSN(ld_cap, {&xrd, &amo_capability})
    DISASM_INSN("lc.cap", lq_cap, 0, 0, {&crd, &amo_capability})
  }
  DEFINE_INSN(lbu_cap, {&xrd, &amo_capability})
  DEFINE_INSN(lhu_cap, {&xrd, &amo_capability})
  if (xlen == 64) {
    DEFINE_INSN(lwu_cap, {&xrd, &amo_capability})
    DEFINE_INSN(ldu_cap, {&xrd, &amo_capability})
  }

  DEFINE_INSN(lr_b_ddc, {&xrd, &amo_address})
  DEFINE_INSN(lr_h_ddc, {&xrd, &amo_address})
  DEFINE_INSN(lr_w_ddc, {&xrd, &amo_address})
  if (xlen == 32) {
    DISASM_INSN("lr.c.ddc", lr_d_ddc, 0, 0, {&crd, &amo_address})
  } else {
    DEFINE_INSN(lr_d_ddc, {&xrd, &amo_address})
    DISASM_INSN("lr.c.ddc", lr_q_ddc, 0, 0, {&crd, &amo_address})
  }

  DEFINE_INSN(lr_b_cap, {&xrd, &amo_capability})
  DEFINE_INSN(lr_h_cap, {&xrd, &amo_capability})
  DEFINE_INSN(lr_w_cap, {&xrd, &amo_capability})
  if (xlen == 32) {
    DISASM_INSN("lr.c.cap", lr_d_ddc, 0, 0, {&crd, &amo_capability})
  } else {
    DEFINE_INSN(lr_d_cap, {&xrd, &amo_capability})
    DISASM_INSN("lr.c.cap", lr_q_ddc, 0, 0, {&crd, &amo_capability})
  }

  /* Memory Stores with Explicit Address Type Instructions */
  DEFINE_INSN(sb_ddc, {&xrs1, &amo_address})
  DEFINE_INSN(sh_ddc, {&xrs1, &amo_address})
  DEFINE_INSN(sw_ddc, {&xrs1, &amo_address})
  if (xlen == 32) {
    DISASM_INSN("sc.ddc", sd_ddc, 0, 0, {&crs2, &amo_address})
  } else {
    DEFINE_INSN(sd_ddc, {&xrs1, &amo_address})
    DISASM_INSN("sc.ddc", sq_ddc, 0, 0, {&crs2, &amo_address})
  }

  DEFINE_INSN(sb_cap, {&xrs1, &amo_capability})
  DEFINE_INSN(sh_cap, {&xrs1, &amo_capability})
  DEFINE_INSN(sw_cap, {&xrs1, &amo_capability})
  if (xlen == 32) {
    DISASM_INSN("sc.cap", sd_cap, 0, 0, {&crs2, &amo_capability})
  } else {
    DEFINE_INSN(sd_cap, {&xrs1, &amo_capability})
    DISASM_INSN("sc.cap", sq_cap, 0, 0, {&crs2, &amo_capability})
  }

  DEFINE_INSN(sc_b_ddc, {&xrs1, &amo_address})
  DEFINE_INSN(sc_h_ddc, {&xrs1, &amo_address})
  DEFINE_INSN(sc_w_ddc, {&xrs1, &amo_address})
  if (xlen == 32) {
    DISASM_INSN("sc.c.ddc", sc_d_ddc, 0, 0, {&crs2, &amo_address})
  } else {
    DEFINE_INSN(sc_d_ddc, {&xrs1, &amo_address})
    DISASM_INSN("sc.c.ddc", sc_q_ddc, 0, 0, {&crs2, &amo_address})
  }

  DEFINE_INSN(sc_b_cap, {&xrs1, &amo_capability})
  DEFINE_INSN(sc_h_cap, {&xrs1, &amo_capability})
  DEFINE_INSN(sc_w_cap, {&xrs1, &amo_capability})
  if (xlen == 32) {
    DISASM_INSN("sc.c.cap", sc_d_ddc, 0, 0, {&crs2, &amo_capability})
  } else {
    DEFINE_INSN(sc_d_cap, {&xrs1, &amo_capability})
    DISASM_INSN("sc.c.cap", sc_q_ddc, 0, 0, {&crs2, &amo_capability})
  }

  /* Memory-Access Instructions */
  if (xlen == 32) {
    DISASM_INSN("lc", ld, 0, 0, {&crd, &load_capability})
    DISASM_INSN("sc", sd, 0, 0, {&crs2, &store_capability})
  } else {
    DISASM_INSN("lc", lq, 0, 0, {&crd, &load_capability})
    DISASM_INSN("sc", sq, 0, 0, {&crs2, &store_capability})
  }

  /* Atomic Memory-Access Instructions */
  if (xlen == 32) {
    DISASM_INSN("lr.c", lr_d, 0, 0, {&crd, &amo_capability})
    DISASM_INSN("sc.c", sc_d, 0, 0, {&xrd, &crs2, &amo_capability})
    DISASM_INSN("amoswap.c", sc_d, 0, 0, {&crd, &crs2, &amo_capability})
  } else {
    DISASM_INSN("lr.c", lr_q, 0, 0, {&crd, &amo_capability})
    DISASM_INSN("sc.c", sc_q, 0, 0, {&xrd, &crs2, &amo_capability})
    DISASM_INSN("amoswap.c", sc_q, 0, 0, {&crd, &crs2, &amo_capability})
  }

  return insns;
}

/* Override extension functions */
void cheri_t::reset() {
#ifdef DEBUG
  fprintf(stderr, "cheri.cc: resetting cheri regs.\n");
#endif //DEBUG

  mem_tags.reset();

  ccsr = 0;

#ifndef CHERI_MERGED_RF
  state.reg_file.reset();
#endif
  state.scrs_reg_file.reset();

  //Taken from Table 5.2 from the cheri architecture spec.
  /* Initialize pcc and ddc */
  /* FIXME: Need to decide what permissions to be set for PCC (i.e. no store) */
  state.scrs_reg_file.write(CHERI_SCR_PCC, CHERI_ALMIGHTY_CAP);
  /* FIXME: Need to decide what permissions to be set for DDC (i.e. no execute); */
  state.scrs_reg_file.write(CHERI_SCR_DDC, CHERI_ALMIGHTY_CAP);

  state.scrs_reg_file.write(CHERI_SCR_UTCC, CHERI_ALMIGHTY_CAP);
  state.scrs_reg_file.write(CHERI_SCR_UTDC, CHERI_NULL_CAP);
  state.scrs_reg_file.write(CHERI_SCR_USCRATCHC, CHERI_NULL_CAP);
  state.scrs_reg_file.write(CHERI_SCR_UEPCC, CHERI_ALMIGHTY_CAP);

  state.scrs_reg_file.write(CHERI_SCR_STCC, CHERI_ALMIGHTY_CAP);
  state.scrs_reg_file.write(CHERI_SCR_STDC, CHERI_NULL_CAP);
  state.scrs_reg_file.write(CHERI_SCR_SSCRATCHC, CHERI_NULL_CAP);
  state.scrs_reg_file.write(CHERI_SCR_SEPCC, CHERI_ALMIGHTY_CAP);

  state.scrs_reg_file.write(CHERI_SCR_MTCC, CHERI_ALMIGHTY_CAP);
  state.scrs_reg_file.write(CHERI_SCR_MTDC, CHERI_NULL_CAP);
  state.scrs_reg_file.write(CHERI_SCR_MSCRATCHC, CHERI_NULL_CAP);
  state.scrs_reg_file.write(CHERI_SCR_MEPCC, CHERI_ALMIGHTY_CAP);

  /* Set cap size to 2*xlen; i.e., 128 cap size for RV64 and 64 for RV32 */
  clen = 2 * p->get_xlen();
};

void cheri_t::set_scr(int index, cheri_reg_t val, processor_t* proc) {
  state.scrs_reg_file.write(index, val);
  reg_t offset = val.offset();
  switch (index) {
    case CHERI_SCR_PCC:
      proc->state.pc = offset;
      break;
    // case CHERI_SCR_UTCC:
    //   proc->state.utvec = offset;
    //   break;
    // case CHERI_SCR_UEPCC:
    //   proc->state.uepc = offset;
    //   break;
    case CHERI_SCR_STCC:
      proc->set_csr(CSR_STVEC, offset);
      break;
    case CHERI_SCR_SEPCC:
      proc->set_csr(CSR_SEPC, offset);
      break;
    case CHERI_SCR_MTCC:
      proc->set_csr(CSR_MTVEC, offset);
      break;
    case CHERI_SCR_MEPCC:
      proc->set_csr(CSR_MEPC, offset);
      break;
    default:
      break;
  }
}

cheri_reg_t cheri_t::get_scr(int index, processor_t* proc) {
  cheri_reg_t ret = CHERI_STATE.scrs_reg_file[index];
  switch (index) {
    case CHERI_SCR_PCC:
      ret.set_offset(proc->state.pc);
      break;
    // case CHERI_SCR_UTCC:
    //   ret.set_offset(proc->state.utvec);
    //   break;
    // case CHERI_SCR_UEPCC:
    //   ret.set_offset(proc->state.uepc);
    //   break;
    case CHERI_SCR_STCC:
      ret.set_offset(proc->get_csr(CSR_STVEC));
      break;
    case CHERI_SCR_SEPCC:
      ret.set_offset(proc->get_csr(CSR_SEPC));
      break;
    case CHERI_SCR_MTCC:
      ret.set_offset(proc->get_csr(CSR_MTVEC));
      break;
    case CHERI_SCR_MEPCC:
      ret.set_offset(proc->get_csr(CSR_MEPC));
      break;
    default:
      break;
  }
  return ret;
}

#endif //ENABLE_CHERI
