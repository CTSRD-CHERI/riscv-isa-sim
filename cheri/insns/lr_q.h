require_extension('A');
require_rv64;
CHERI_MODE_ACQUIRE_LOAD_RESERVATION(cap, insn.rs1(), 0);
WRITE_CD(CHERI_MODE_LOAD(cap, insn.rs1(), 0));
