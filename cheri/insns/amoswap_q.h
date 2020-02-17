require_extension('A');
require_rv64;
if (CHERI->get_mode()) {
  WRITE_CD(CHERI->cap_amoswap_cap(CS1, insn.rs1(), 0, CS2));
} else {
  WRITE_CD(CHERI->ddc_amoswap_cap(RS1, CS2));
}
