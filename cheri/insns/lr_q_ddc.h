require_extension('A');
require_rv64;
CHERI->ddc_acquire_load_reservation_cap(RS1);
WRITE_CD(CHERI->ddc_load_cap(RS1));
