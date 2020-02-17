require_extension('A');
require_rv64;

bool have_reservation = CHERI->cap_check_load_reservation_cap(CS1, insn.rs1(), 0, CS2);
if (have_reservation) CHERI->cap_store_cap(CS1, insn.rs1(), 0, CS2);
MMU.yield_load_reservation();

WRITE_CS2(cheri_reg_t(!have_reservation));
