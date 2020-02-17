require_extension('A');
WRITE_RD(sext32(CHERI_MODE_AMO(uint32, insn.rs1(), 0, [&](uint32_t lhs) { return RS2; })));
