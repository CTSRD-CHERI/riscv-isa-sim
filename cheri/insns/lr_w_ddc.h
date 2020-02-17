require_extension('A');
CHERI->ddc_acquire_load_reservation_int32(RS1);
WRITE_RD(CHERI->ddc_load_int32(RS1));
