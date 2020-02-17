require_extension('A');

#ifndef CHERI_MERGED_RF
require_rv64;
#else
if (xlen == 32) { /* sc.c.ddc */
  bool have_reservation = CHERI->ddc_check_load_reservation_cap(RS1, CS2);
  if (have_reservation) CHERI->ddc_store_cap(RS1, CS2);
  MMU.yield_load_reservation();

  WRITE_CS2(cheri_reg_t(!have_reservation));
} else
#endif
{
  bool have_reservation = CHERI->ddc_check_load_reservation_uint64(RS1, RS2);
  if (have_reservation) CHERI->ddc_store_uint64(RS1, RS2);
  MMU.yield_load_reservation();

  WRITE_RS2(!have_reservation);
}
