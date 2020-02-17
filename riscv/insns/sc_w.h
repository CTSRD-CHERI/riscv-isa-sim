require_extension('A');

bool have_reservation = CHERI_MODE_CHECK_LOAD_RESERVATION(uint32, insn.rs1(), 0, RS2);
if (have_reservation) CHERI_MODE_STORE(uint32, insn.rs1(), 0, RS2);
MMU.yield_load_reservation();

WRITE_RD(!have_reservation);
