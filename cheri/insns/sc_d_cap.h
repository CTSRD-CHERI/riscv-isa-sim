require_extension('A');

#ifndef CHERI_MERGED_RF
require_rv64;
#else
if (xlen == 32) { /* sc.c.cap */
  bool have_reservation = CHERI->cap_check_load_reservation_cap(CS1, insn.rs1(), 0, CS2);
  if (have_reservation) CHERI->cap_store_cap(CS1, insn.rs1(), 0, CS2);
  MMU.yield_load_reservation();

  WRITE_CS2(cheri_reg_t(!have_reservation));
} else
#endif
{
  bool have_reservation = CHERI->cap_check_load_reservation_uint64(CS1, insn.rs1(), 0, RS2);
  if (have_reservation) CHERI->cap_store_uint64(CS1, insn.rs1(), 0, RS2);
  MMU.yield_load_reservation();

  WRITE_RS2(!have_reservation);
}
