require_extension('A');

#ifndef CHERI_MERGED_RF
require_rv64;
#else
if (xlen == 32) { /* sc.c */
  bool have_reservation = CHERI_MODE_CHECK_LOAD_RESERVATION(cap, insn.rs1(), 0, CS2);
  if (have_reservation) CHERI_MODE_STORE(cap, insn.rs1(), 0, CS2);
  MMU.yield_load_reservation();

  WRITE_RD(!have_reservation);
} else
#endif
{
  bool have_reservation = CHERI_MODE_CHECK_LOAD_RESERVATION(uint64, insn.rs1(), 0, RS2);
  if (have_reservation) CHERI_MODE_STORE(uint64, insn.rs1(), 0, RS2);
  MMU.yield_load_reservation();

  WRITE_RD(!have_reservation);
}
