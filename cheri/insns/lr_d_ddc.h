require_extension('A');

if (xlen == 32) { /* lr.c.ddc */
  CHERI->ddc_acquire_load_reservation_cap(RS1);
  WRITE_CD(CHERI->ddc_load_cap(RS1));
} else {
  CHERI->ddc_acquire_load_reservation_int64(RS1);
  WRITE_RD(CHERI->ddc_load_int64(RS1));
}
