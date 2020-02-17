require_extension('A');

if (xlen == 32) { /* lr.c.cap */
  CHERI->cap_acquire_load_reservation_cap(RS1, insn.rs1(), 0);
  WRITE_CD(CHERI->cap_load_cap(RS1, insn.rs1(), 0));
} else {
  CHERI->cap_acquire_load_reservation_int64(RS1, insn.rs1(), 0);
  WRITE_RD(CHERI->cap_load_int64(RS1, insn.rs1(), 0));
}
