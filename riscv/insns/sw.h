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

#ifdef CHERI_MERGED_RF
if (CHERI->get_mode()) {
  if (!CS1.tag) {
  #if DEBUG
    printf("CHERI: Trying to store via untagged cap register\n");
  #endif

    CHERI->raise_trap(CAUSE_CHERI_TAG_FAULT, insn.cs1());
  } else if (CS1.sealed) {
  #if DEBUG
    printf("CHERI: Trying to store via a sealed cap register\n");
  #endif
    CHERI->raise_trap(CAUSE_CHERI_SEAL_FAULT, insn.cs1());
  } else if ((CS1.perms & BIT(CHERI_PERMIT_STORE)) != BIT(CHERI_PERMIT_STORE)) {
  #if DEBUG
    printf("CHERI: Trying to store with no cap STORE permissions\n");
  #endif
    CHERI->raise_trap(CAUSE_CHERI_PERMIT_STORE_FAULT, insn.cs1());
  }

  reg_t addr = CS1.base + CS1.offset + insn.s_imm();

  if (addr + 4 > CS1.base + CS1.length) {
  #if DEBUG
    printf("CHERI: Trying to store with wrong bounds\n");
  #endif
    CHERI->raise_trap(CAUSE_CHERI_LENGTH_FAULT, insn.cs1());
  } else if (addr < CS1.base) {
  #if DEBUG
    printf("CHERI: Trying to store with wrong bounds\n");
  #endif
    CHERI->raise_trap(CAUSE_CHERI_LENGTH_FAULT, insn.cs1());
  } else {
  #if DEBUG
    printf("CHERI: storing mem \n");
  #endif
    CHERI->cheriMem_clearTag(addr);
    CHERI->get_mmu()->store_uint32(addr, READ_REG(insn.rs2()));
  }
} else {
  MMU.store_uint32(RS1 + insn.s_imm(), RS2);
}
#else
MMU.store_uint32(RS1 + insn.s_imm(), RS2);
#endif
