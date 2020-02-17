require_extension('A');
require_rv64;

bool have_reservation = CHERI->ddc_check_load_reservation_cap(RS1, CS2);
if (have_reservation) CHERI->ddc_store_cap(RS1, CS2);
MMU.yield_load_reservation();

WRITE_CS2(cheri_reg_t(!have_reservation));
