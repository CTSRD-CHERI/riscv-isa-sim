require_extension('A');
CHERI->cap_acquire_load_reservation_int32(RS1, insn.rs1(), 0);
WRITE_RD(CHERI->cap_load_int32(RS1, insn.rs1(), 0));
