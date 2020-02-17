require_extension('A');
require_rv64;

bool have_reservation = CHERI_MODE_CHECK_LOAD_RESERVATION(cap, insn.rs1(), 0, CS2);
if (have_reservation) CHERI_MODE_STORE(cap, insn.rs1(), 0, CS2);
MMU.yield_load_reservation();

WRITE_RD(!have_reservation);
