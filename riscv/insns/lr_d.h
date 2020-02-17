require_extension('A');

#ifndef CHERI_MERGED_RF
require_rv64;
#else
if (xlen == 32) { /* lr.c */
  CHERI_MODE_ACQUIRE_LOAD_RESERVATION(cap, insn.rs1(), 0);
  WRITE_CD(CHERI_MODE_LOAD(cap, insn.rs1(), 0));
} else
#endif
{
  CHERI_MODE_ACQUIRE_LOAD_RESERVATION(int64, insn.rs1(), 0);
  WRITE_RD(CHERI_MODE_LOAD(int64, insn.rs1(), 0));
}
