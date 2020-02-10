// See LICENSE for license details.
// See LICENSE_CHERI for license details.

#include "decode.h"
#include "disasm.h"
#include "sim.h"
#include "mmu.h"
#include <sys/mman.h>
#include <termios.h>
#include <map>
#include <iostream>
#include <climits>
#include <cinttypes>
#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <math.h>

#ifdef ENABLE_CHERI
#include "cheri.h"
#endif

DECLARE_TRAP(-1, interactive)

processor_t *sim_t::get_core(const std::string& i)
{
  char *ptr;
  unsigned long p = strtoul(i.c_str(), &ptr, 10);
  if (*ptr || p >= procs.size())
    throw trap_interactive();
  return get_core(p);
}

static std::string readline(int fd)
{
  struct termios tios;
  bool noncanonical = tcgetattr(fd, &tios) == 0 && (tios.c_lflag & ICANON) == 0;

  std::string s;
  while (true)
  {
    int ch = getc(stdin);
    if (ch == EOF) {
      if (feof(stdin)) {
        fprintf(stderr, "\nReceived EOF. Hit CTRL+C twice to exit (ctrlc_pressed=%d)\n", ctrlc_pressed);
        clearerr(stdin);
        break;
      }
      assert(ferror(stdin));
      err(EX_OSERR, "Failed to read from stdin");
    }
    if (ch == '\x7f')
    {
      if (s.empty())
        continue;
      s.erase(s.end()-1);

      if (noncanonical && write(fd, "\b \b", 3) != 3)
        ; // shut up gcc
    }
    else if (noncanonical && write(fd, &ch, 1) != 1)
      ; // shut up gcc

    if (ch == '\n')
      break;
    if (ch != '\x7f')
      s += ch;
  }
  return s;
}

void sim_t::interactive()
{
  typedef void (sim_t::*interactive_func)(const std::string&, const std::vector<std::string>&);
  std::map<std::string,interactive_func> funcs;

  funcs["run"] = &sim_t::interactive_run_noisy;
  funcs["r"] = funcs["run"];
  funcs["rs"] = &sim_t::interactive_run_silent;
  funcs["vreg"] = &sim_t::interactive_vreg;
  funcs["reg"] = &sim_t::interactive_reg;
  funcs["freg"] = &sim_t::interactive_freg;
#ifdef ENABLE_CHERI
  funcs["creg"] = &sim_t::interactive_creg;
#endif //ENABLE_CHERI
  funcs["fregs"] = &sim_t::interactive_fregs;
  funcs["fregd"] = &sim_t::interactive_fregd;
  funcs["pc"] = &sim_t::interactive_pc;
  funcs["mem"] = &sim_t::interactive_mem;
  funcs["str"] = &sim_t::interactive_str;
  funcs["dis"] = &sim_t::interactive_dis;
  funcs["until"] = &sim_t::interactive_until_silent;
  funcs["untiln"] = &sim_t::interactive_until_noisy;
  funcs["while"] = &sim_t::interactive_until_silent;
  funcs["quit"] = &sim_t::interactive_quit;
  funcs["q"] = funcs["quit"];
  funcs["help"] = &sim_t::interactive_help;
  funcs["h"] = funcs["help"];

  while (!done())
  {
    std::cerr << ": " << std::flush;
    std::string s = readline(STDERR_FILENO);

    std::stringstream ss(s);
    std::string cmd, tmp;
    std::vector<std::string> args;

    if (!(ss >> cmd))
    {
      set_procs_debug(true);
      step(1);
      continue;
    }

    while (ss >> tmp)
      args.push_back(tmp);

    try
    {
      if(funcs.count(cmd))
        (this->*funcs[cmd])(cmd, args);
      else
        fprintf(stderr, "Unknown command %s\n", cmd.c_str());
    }
    catch(trap_t& t) {}
  }
  ctrlc_pressed = false;
}

void sim_t::interactive_help(const std::string& cmd, const std::vector<std::string>& args)
{
  std::cerr <<
    "Interactive commands:\n"
    "reg <core> [reg]                # Display [reg] (all if omitted) in <core>\n"
    "fregs <core> <reg>              # Display single precision <reg> in <core>\n"
    "fregd <core> <reg>              # Display double precision <reg> in <core>\n"
    "vreg <core> [reg]               # Display vector [reg] (all if omitted) in <core>\n"
    "pc <core>                       # Show current PC in <core>\n"
    "mem [core] <hex addr>           # Show contents of memory (physical if [core] omitted)\n"
    "str [core] <hex addr>           # Show NUL-terminated C string\n"
    "dis <core> <hex addr> [count]   # Disassemble [count] (1 if omitted) instructions\n"
    "until reg <core> <reg> <val>    # Stop when <reg> in <core> hits <val>\n"
    "until pc <core> <val>           # Stop when PC in <core> hits <val>\n"
    "until [m|s]int <core> <val>     # Stop when an interrupt <val> get triggered in <core> in [s|m] mode\n"
    "until [m|s]exc <core> <val>     # Stop when an exception <val> get triggered in <core> in [s|m] mode\n"
    "untiln pc <core> <val>          # Run noisy and stop when PC in <core> hits <val>\n"
    "until mem [core] <addr> <val>   # Stop when memory <addr> becomes <val>\n"
    "while reg <core> <reg> <val>    # Run while <reg> in <core> is <val>\n"
    "while pc <core> <val>           # Run while PC in <core> is <val>\n"
    "while mem [core] <addr> <val>   # Run while memory <addr> is <val>\n"
    "run [count]                     # Resume noisy execution (until CTRL+C, or [count] insns)\n"
    "r [count]                         Alias for run\n"
    "rs [count]                      # Resume silent execution (until CTRL+C, or [count] insns)\n"
    "quit                            # End the simulation\n"
    "q                                 Alias for quit\n"
    "help                            # This screen!\n"
    "h                                 Alias for help\n"
    "Note: Hitting enter is the same as: run 1\n"
    << std::flush;
}

void sim_t::interactive_run_noisy(const std::string& cmd, const std::vector<std::string>& args)
{
  interactive_run(cmd,args,true);
}

void sim_t::interactive_run_silent(const std::string& cmd, const std::vector<std::string>& args)
{
  interactive_run(cmd,args,false);
}

void sim_t::interactive_run(const std::string& cmd, const std::vector<std::string>& args, bool noisy)
{
  size_t steps = args.size() ? atoll(args[0].c_str()) : -1;
  ctrlc_pressed = false;
  set_procs_debug(noisy);
  for (size_t i = 0; i < steps && !ctrlc_pressed && !done(); i++)
    step(1);
}

void sim_t::interactive_quit(const std::string& cmd, const std::vector<std::string>& args)
{
  exit(0);
}

reg_t sim_t::get_pc(const std::vector<std::string>& args)
{
  if(args.size() != 1)
    throw trap_interactive();

  processor_t *p = get_core(args[0]);
  return p->get_state()->pc;
}

reg_t sim_t::get_inst(const std::vector<std::string>& args)
{
  if(args.size() != 1)
    throw trap_interactive();

  processor_t *p = get_core(args[0]);
  return (reg_t) p->get_mmu()->get_insn(p->get_state()->pc);
}

void sim_t::interactive_pc(const std::string& cmd, const std::vector<std::string>& args)
{
  fprintf(stderr, "0x%016" PRIx64 "\n", get_pc(args));
}

reg_t sim_t::get_reg(const std::vector<std::string>& args)
{
  if(args.size() != 2)
    throw trap_interactive();

  processor_t *p = get_core(args[0]);

  unsigned long r = std::find(xpr_name, xpr_name + NXPR, args[1]) - xpr_name;
  if (r == NXPR) {
    char *ptr;
    r = strtoul(args[1].c_str(), &ptr, 10);
    if (*ptr) {
      #define DECLARE_CSR(name, number) if (args[1] == #name) return p->get_csr(number);
      #include "encoding.h"              // generates if's for all csrs
      r = NXPR;                          // else case (csr name not found)
      #undef DECLARE_CSR
    }
  }

  if (r >= NXPR)
    throw trap_interactive();

  return READ_REG(r);
}

#ifdef ENABLE_CHERI
cheri_reg_t sim_t::get_creg(const std::vector<std::string>& args)
{
  if(args.size() != 2)
    throw trap_interactive();

  processor_t *p = get_core(args[0]);
  cheri_t *cheri = (static_cast<cheri_t*>(p->get_extension()));

  unsigned long r = std::find(cheri_reg_names, cheri_reg_names + NXPR, args[1]) - cheri_reg_names;
  if (r == NXPR) {
    char *ptr;
    r = strtoul(args[1].c_str(), &ptr, 10);
    if (*ptr) {
      #define DECLARE_CHERI_SCR(name, number) if (args[1] == #name) return cheri->get_scr(number, p);
      #include "encoding.h"              // generates if's for all scrs
      r = NXPR;                          // else case (scr name not found)
      #undef DECLARE_CHERI_SCR
    }
  }

  if (r >= NXPR)
    throw trap_interactive();

  return READ_CREG(r);
}
#endif //ENABLE_CHERI

freg_t sim_t::get_freg(const std::vector<std::string>& args)
{
  if(args.size() != 2)
    throw trap_interactive();

  processor_t *p = get_core(args[0]);
  int r = std::find(fpr_name, fpr_name + NFPR, args[1]) - fpr_name;
  if (r == NFPR)
    r = atoi(args[1].c_str());
  if (r >= NFPR)
    throw trap_interactive();

  return p->get_state()->FPR[r];
}

void sim_t::interactive_vreg(const std::string& cmd, const std::vector<std::string>& args)
{
  int rstart = 0;
  int rend = NVPR;
  if (args.size() >= 2) {
    rstart = strtol(args[1].c_str(), NULL, 0);
    if (!(rstart >= 0 && rstart < NVPR)) {
      rstart = 0;
    } else {
      rend = rstart + 1;
    }
  }

  // Show all the regs!
  processor_t *p = get_core(args[0]);
  const int vlen = (int)(p->VU.get_vlen()) >> 3;
  const int elen = (int)(p->VU.get_elen()) >> 3;
  const int num_elem = vlen/elen;
  fprintf(stderr, "VLEN=%d bits; ELEN=%d bits\n", vlen << 3, elen << 3);

  for (int r = rstart; r < rend; ++r) {
    fprintf(stderr, "%-4s: ", vr_name[r]);
    for (int e = num_elem-1; e >= 0; --e){
      uint64_t val;
      switch(elen){
        case 8:
          val = P.VU.elt<uint64_t>(r, e);
          fprintf(stderr, "[%d]: 0x%016" PRIx64 "  ", e, val);
          break;
        case 4:
          val = P.VU.elt<uint32_t>(r, e);
          fprintf(stderr, "[%d]: 0x%08" PRIx32 "  ", e, (uint32_t)val);
          break;
        case 2:
          val = P.VU.elt<uint16_t>(r, e);
          fprintf(stderr, "[%d]: 0x%08" PRIx16 "  ", e, (uint16_t)val);
          break;
        case 1:
          val = P.VU.elt<uint8_t>(r, e);
          fprintf(stderr, "[%d]: 0x%08" PRIx8 "  ", e, (uint8_t)val);
          break;
      }
    }
    fprintf(stderr, "\n");
  }
}


void sim_t::interactive_reg(const std::string& cmd, const std::vector<std::string>& args)
{
  if (args.size() == 1) {
    // Show all the regs!
    processor_t *p = get_core(args[0]);

    for (int r = 0; r < NXPR; ++r) {

#ifdef CHERI_MERGED_RF
      fprintf(stderr, "%-4s: 0x%016" PRIx64 "  ", xpr_name[r], p->get_state()->XPR[r].cursor());
#else //CHERI_MERGED_RF
      fprintf(stderr, "%-4s: 0x%016" PRIx64 "  ", xpr_name[r], p->get_state()->XPR[r]);
#endif //CHERI_MERGED_RF

      if ((r + 1) % 4 == 0)
        fprintf(stderr, "\n");
    }
  } else
    fprintf(stderr, "0x%016" PRIx64 "\n", get_reg(args));
}

union fpr
{
  freg_t r;
  float s;
  double d;
};

void sim_t::interactive_freg(const std::string& cmd, const std::vector<std::string>& args)
{
  freg_t r = get_freg(args);
  fprintf(stderr, "0x%016" PRIx64 "%016" PRIx64 "\n", r.v[1], r.v[0]);
}

void sim_t::interactive_fregs(const std::string& cmd, const std::vector<std::string>& args)
{
  fpr f;
  f.r = get_freg(args);
  fprintf(stderr, "%g\n", isBoxedF32(f.r) ? (double)f.s : NAN);
}

static void print_creg(cheri_reg_t creg, const char *name)
{
  if (name)
    fprintf(stderr, "%-5s: ", name);

  fprintf(stderr, "v:%" PRIu32 " f:%" PRIu32 " p:%016" PRIx64
         " b:%016" PRIx64 " l:%1" PRIx64 "%016" PRIx64 " c:%016" PRIx64
         " t:%08" PRIx32 "\n",
         creg.tag, creg.flags,
         (((uint64_t)creg.uperms << CHERI_USER_PERM_SHIFT) | creg.perms),
         creg.base(), (uint64_t)(creg.length() >> 64),
         (uint64_t)(creg.length() & UINT64_MAX), creg.cursor(), creg.otype);
}

#ifdef ENABLE_CHERI
void sim_t::interactive_creg(const std::string& cmd, const std::vector<std::string>& args)
{
  if (args.size() == 1) {
    cheri_reg_t creg;
    // Show all the regs!
    processor_t *p = get_core(args[0]);

    for (int r = 0; r < NUM_CHERI_REGS; ++r) {
      creg = READ_CREG(r);

      print_creg(creg, cheri_reg_names[r]);
    }
  } else {
    cheri_reg_t creg = get_creg(args);
    print_creg(creg, NULL);
  }
}
#endif //ENABLE_CHERI

void sim_t::interactive_fregd(const std::string& cmd, const std::vector<std::string>& args)
{
  fpr f;
  f.r = get_freg(args);
  fprintf(stderr, "%g\n", isBoxedF64(f.r) ? f.d : NAN);
}

reg_t sim_t::get_mem(const std::vector<std::string>& args)
{
  if(args.size() != 1 && args.size() != 2)
    throw trap_interactive();

  std::string addr_str = args[0];
  mmu_t* mmu = debug_mmu;
  if(args.size() == 2)
  {
    processor_t *p = get_core(args[0]);
    mmu = p->get_mmu();
    addr_str = args[1];
  }

  reg_t addr = strtol(addr_str.c_str(),NULL,16), val;
  if(addr == LONG_MAX)
    addr = strtoul(addr_str.c_str(),NULL,16);

  switch(addr % 8)
  {
    case 0:
      val = mmu->load_uint64(addr);
      break;
    case 4:
      val = mmu->load_uint32(addr);
      break;
    case 2:
    case 6:
      val = mmu->load_uint16(addr);
      break;
    default:
      val = mmu->load_uint8(addr);
      break;
  }
  return val;
}

void sim_t::interactive_mem(const std::string& cmd, const std::vector<std::string>& args)
{
  fprintf(stderr, "0x%016" PRIx64 "\n", get_mem(args));
}

void sim_t::interactive_str(const std::string& cmd, const std::vector<std::string>& args)
{
  if(args.size() != 1 && args.size() != 2)
    throw trap_interactive();

  std::string addr_str = args[0];
  mmu_t* mmu = debug_mmu;
  if(args.size() == 2)
  {
    processor_t *p = get_core(args[0]);
    mmu = p->get_mmu();
    addr_str = args[1];
  }

  reg_t addr = strtol(addr_str.c_str(),NULL,16);
  if(addr == LONG_MAX)
    addr = strtoul(addr_str.c_str(),NULL,16);

  char ch;
  while((ch = mmu->load_uint8(addr++)))
    putchar(ch);

  putchar('\n');
}

void sim_t::interactive_dis(const std::string& cmd, const std::vector<std::string>& args)
{
  if (args.size() != 2 && args.size() != 3)
    throw trap_interactive();

  processor_t *p = get_core(args[0]);
  mmu_t* mmu = p->get_mmu();

  reg_t addr = strtol(args[1].c_str(),NULL,16);
  if (addr == LONG_MAX)
    addr = strtoul(args[1].c_str(),NULL,16);

  unsigned long count = 1;
  if (args.size() == 3)
    count = strtoul(args[2].c_str(), NULL, 10);

  for (; count > 0; --count) {
    insn_bits_t insn = mmu->get_insn(addr);
    uint64_t bits = insn & ((1ULL << (8 * insn_length(insn))) - 1);
    fprintf(stderr, "0x%016" PRIx64 " (0x%08" PRIx64 ") %s\n",
            addr, bits, p->get_disassembler()->disassemble(insn).c_str());
    addr += insn_length(insn);
  }
}

void sim_t::interactive_until_silent(const std::string& cmd, const std::vector<std::string>& args)
{
  interactive_until(cmd, args, false);
}

void sim_t::interactive_until_noisy(const std::string& cmd, const std::vector<std::string>& args)
{
  interactive_until(cmd, args, true);
}

void sim_t::interactive_until(const std::string& cmd, const std::vector<std::string>& args, bool noisy)
{
  bool cmd_until = cmd == "until" || cmd == "untiln";
  bool until_mtrap = args[0] == "mint" || args[0] == "mexc";
  bool until_strap = args[0] == "sint" || args[0] == "sexc";

  if(args.size() < 3)
    return;

  reg_t val = strtol(args[args.size()-1].c_str(),NULL,16);
  if(val == LONG_MAX)
    val = strtoul(args[args.size()-1].c_str(),NULL,16);

  std::vector<std::string> args2;
  args2 = std::vector<std::string>(args.begin()+1,args.end()-1);

  auto func = args[0] == "reg" ? &sim_t::get_reg :
              args[0] == "pc"  ? &sim_t::get_pc :
              args[0] == "inst"  ? &sim_t::get_inst :
              args[0] == "mem" ? &sim_t::get_mem :
              args[0] == "mint" ? &sim_t::get_pc :
              args[0] == "mexc" ? &sim_t::get_pc :
              args[0] == "sint" ? &sim_t::get_pc :
              args[0] == "sexc" ? &sim_t::get_pc :
              NULL;

  if (func == NULL)
    return;

  ctrlc_pressed = false;

  while (1)
  {
    try
    {
      processor_t *p = get_core(args[1]);
      reg_t current = (this->*func)(args2);
      reg_t mtvec = p->get_state()->mtvec;
      reg_t mcause = p->get_state()->mcause;
      reg_t stvec = p->get_state()->stvec;
      reg_t scause = p->get_state()->scause;

      if (cmd_until) {
        if (current == val)
          break;
        if (until_mtrap && (current == mtvec) && (val == (mcause & 0xff)) && args[0] == "mexc" && !(mcause >> (p->get_xlen()-1)))
          break;
        if (until_strap && (current == stvec) && (val == (scause & 0xff)) && args[0] == "sexc" && !(scause >> (p->get_xlen()-1)))
          break;
        if (until_mtrap && (current == mtvec) && (val == (mcause & 0xff)) && args[0] == "mint" && (mcause >> (p->get_xlen()-1)))
          break;
        if (until_strap && (current == stvec) && (val == (scause & 0xff)) && args[0] == "sint" && (scause >> (p->get_xlen()-1)))
          break;
        if (ctrlc_pressed)
          break;
      }

      if (ctrlc_pressed)
        break;
    }
    catch (trap_t& t) {}

    set_procs_debug(noisy);
    step(1);
  }
}
