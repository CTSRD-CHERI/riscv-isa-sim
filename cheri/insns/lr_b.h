require_extension('A');
CHERI_MODE_ACQUIRE_LOAD_RESERVATION(int8, insn.rs1(), 0);
WRITE_RD(CHERI_MODE_LOAD(int8, insn.rs1(), 0));
