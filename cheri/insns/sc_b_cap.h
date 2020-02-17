require_extension('A');

bool have_reservation = CHERI->cap_check_load_reservation_uint8(CS1, insn.rs1(), 0, RS2);
if (have_reservation) CHERI->cap_store_uint8(CS1, insn.rs1(), 0, RS2);
MMU.yield_load_reservation();

WRITE_RS2(!have_reservation);
