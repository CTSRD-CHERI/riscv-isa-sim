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

#ifndef _RISCV_CHERI_TYPES_H
#define _RISCV_CHERI_TYPES_H

#include <config.h>
#include <stdint.h>

/* Use __uint128 to represent 65 bit length */
__extension__ typedef unsigned __int128 cheri_length_t;

/* The architectural permissions bits, may be greater than the number of bits
 * actually available in the encoding. */
#define CHERI_USER_PERM_BITS  4
#define CHERI_USER_PERM_SHIFT 15
#define CHERI_PERM_BITS       12

#define OTYPE_RESERVED_COUNT 4
#define OTYPE_UNSEALED 0x3ffffu
#define OTYPE_SENTRY 0x3fffeu
#define OTYPE_MAX (0x3ffffu - OTYPE_RESERVED_COUNT)

#define MAX_CHERI_LENGTH ((cheri_length_t)1u << 64)

/* CHERI compressed format in memory */
struct cheri_reg_inmem_t {
 public:
  uint64_t cursor;
  uint64_t pesbt;
};

struct cap_register;

struct cheri_reg_t {
 private:
  static const uint32_t reset_ebt;

  /* Cached; must go through mutation functions */
  uint64_t _base;
  cheri_length_t _top;
  /* Uncached, but affects cached values */
  uint64_t _cursor;
  uint32_t _ebt;

  struct cap_register cap_lib() const;
  void set_cap_lib(const struct cap_register &cap);

 public:
  uint32_t flags  : 1;
  uint32_t uperms : CHERI_USER_PERM_BITS;
  uint32_t perms  : CHERI_PERM_BITS;

  uint32_t otype    : 24;
  uint32_t reserved : 8;
  uint32_t tag      : 1;

  cheri_reg_t(const cheri_reg_t&) = default;
  cheri_reg_t(cheri_reg_t&&) = default;
  cheri_reg_t(const cheri_reg_inmem_t &inmem, bool tag);

  cheri_reg_t& operator=(const cheri_reg_t&) = default;
  cheri_reg_t& operator=(cheri_reg_t&&) = default;

  uint64_t base() const { return _base; }
  cheri_length_t top() const { return _top; }
  uint64_t cursor() const { return _cursor; }

  uint64_t offset() const { return _cursor - _base; }
  cheri_length_t length() const { return _top - _base; }
  bool sealed() const { return ((otype != OTYPE_UNSEALED) && (otype != OTYPE_SENTRY)); }
  bool sentry() const { return otype == OTYPE_SENTRY; }

  cheri_reg_inmem_t inmem() const;
  void set_bounds(uint64_t base, cheri_length_t top);
  void set_cursor(uint64_t cursor);
  void inc_offset(uint64_t inc) {
    set_cursor(_cursor + inc);
  }
  void set_offset(uint64_t offset) {
    set_cursor(_base + offset);
  }

  static uint64_t representable_length(uint64_t len);
  static uint64_t representable_alignment_mask(uint64_t len);

  cheri_reg_t(uint64_t cursor = 0) {
    _base    = 0;
    _top     = MAX_CHERI_LENGTH;
    _cursor  = cursor;
    _ebt     = reset_ebt;
    flags    = 0;
    uperms   = 0;
    perms    = 0;
    otype    = OTYPE_UNSEALED;
    reserved = 0;
    tag      = 0;
  }

  static cheri_reg_t almighty(uint64_t cursor = 0) {
    cheri_reg_t ret;
    ret._base    = 0;
    ret._top     = MAX_CHERI_LENGTH;
    ret._cursor  = cursor;
    ret._ebt     = reset_ebt;
    ret.flags    = 0;
    ret.uperms   = (1 << CHERI_USER_PERM_BITS) - 1;
    ret.perms    = (1 << CHERI_PERM_BITS) - 1;
    ret.otype    = OTYPE_UNSEALED;
    ret.reserved = 0;
    ret.tag      = 1;
    return ret;
  }
};

#define CHERI_NULL_CAP (cheri_reg_t())
#define CHERI_ALMIGHTY_CAP (cheri_reg_t::almighty())
#endif
