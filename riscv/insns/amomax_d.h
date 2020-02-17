require_extension('A');
require_rv64;
WRITE_RD(CHERI_MODE_AMO(uint64, insn.rs1(), 0, [&](int64_t lhs) { return std::max(lhs, int64_t(RS2)); }));
