require_extension('A');

bool have_reservation = CHERI->ddc_check_load_reservation_uint32(RS1, RS2);
if (have_reservation) CHERI->ddc_store_uint32(RS1, RS2);
MMU.yield_load_reservation();

WRITE_RS2(!have_reservation);
