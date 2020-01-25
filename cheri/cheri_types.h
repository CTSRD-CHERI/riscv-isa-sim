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

/* Use __uint128 to represent 65 bit length */
__extension__ typedef unsigned __int128 cheri_length_t;

/* The architectural permissions bits, may be greater than the number of bits
 * actually available in the encoding. */
#define CHERI_USER_PERM_BITS 16
#define CHERI_PERM_BITS      15

#define OTYPE_UNSEALED 0x3ffffu
#define OTYPE_MAX 0x3ffff

/* 257-bit Caps register format *
 * ----------------------------------------------------------------------------
 * | base | length | cursor | flags | uperms | perms | otype | reserved | tag |
 * ----------------------------------------------------------------------------
 * |  64  |   64   |   64   |   1   |   16   |  15   |   24  |    8     |  1  |
 * ----------------------------------------------------------------------------
 */
struct cheri_reg_t {
 public:
  uint64_t base;
  cheri_length_t length;
  uint64_t cursor;

  uint32_t flags  : 1;
  uint32_t uperms : CHERI_USER_PERM_BITS;
  uint32_t perms  : CHERI_PERM_BITS;

  uint32_t otype    : 24;
  uint32_t reserved : 8;
  uint32_t tag      : 1;

  bool sealed() const { return otype != OTYPE_UNSEALED; }
};

#define MAX_CHERI_LENGTH ((cheri_length_t)1u << 64)

#define CHERI_NULL_CAP (cheri_reg_t) { \
  .base     = 0,                       \
  .length   = MAX_CHERI_LENGTH,      \
  .cursor   = 0,                       \
  .flags    = 0,                       \
  .uperms   = 0,                       \
  .perms    = 0,                       \
  .otype    = OTYPE_UNSEALED,          \
  .reserved = 0,                       \
  .tag      = 0                        \
}

#define CHERI_ALMIGHTY_CAP (cheri_reg_t) { \
  .base     = 0,                           \
  .length   = MAX_CHERI_LENGTH,          \
  .cursor   = 0,                           \
  .flags    = 0,                           \
  .uperms   = 0xfu,                        \
  .perms    = 0xfffu,                      \
  .otype    = OTYPE_UNSEALED,              \
  .reserved = 0,                           \
  .tag      = 1                            \
}

#ifdef ENABLE_CHERI128
//typedef cheri_reg_t cap_register_t;
//#define  HAVE_CAP_REGISTER_T 1

/* CHERI compressed format in memory */
struct cheri_reg_inmem_t {
 public:
  uint64_t cursor;
  uint64_t pesbt;
};
#else
/* In memory is same as in register (except for tag) */
typedef cheri_reg_t cheri_reg_inmem_t;
#endif


#endif
