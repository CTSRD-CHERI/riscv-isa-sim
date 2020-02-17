require_extension('A');
require_rv64;
CHERI->cap_acquire_load_reservation_cap(RS1, insn.rs1(), 0);
WRITE_CD(CHERI->cap_load_cap(RS1, insn.rs1(), 0));
