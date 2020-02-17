require_extension('A');
#ifndef CHERI_MERGED_RF
require_rv64;
#else
if (xlen == 32) { /* amoswap.c */
  if (CHERI->get_mode()) {
    WRITE_CD(CHERI->cap_amoswap_cap(CS1, insn.rs1(), 0, CS2));
  } else {
    WRITE_CD(CHERI->ddc_amoswap_cap(RS1, CS2));
  }
} else
#endif
{
  WRITE_RD(CHERI_MODE_AMO(uint64, insn.rs1(), 0, [&](uint64_t lhs) { return RS2; }));
}
