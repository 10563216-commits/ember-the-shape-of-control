






#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using Byte = std::uint8_t;
using Addr = std::uint16_t;

constexpr std::size_t MEM_SIZE   = 4096;      
constexpr unsigned long STEP_LIMIT = 100000;  



struct Memory {
    std::array<Byte, MEM_SIZE> bytes{};
};

inline Byte mem_get(const Memory& m, Addr a) { return m.bytes[a]; }
inline void mem_set(Memory& m, Addr a, Byte v) { m.bytes[a] = v; }



struct CPU {
    Memory mem;
    Byte a = 0, b = 0;
    Addr h = 0;
    Addr pc = 0x000;
    Addr sp = 0xFFF;
    bool z = false, n = false, c = false;
    bool halted = false;
    std::string error;          
    unsigned long steps = 0;    
    std::string out;            
};

static std::string hex(unsigned v, int width) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%0*X", width, v);
    return buf;
}

static void fault(CPU& cpu, const std::string& msg) {
    cpu.halted = true;
    cpu.error = msg;
}

static void set_zn(CPU& cpu, Byte r) {
    cpu.z = (r == 0);
    cpu.n = (r & 0x80) != 0;
}


static bool fetch(CPU& cpu, std::uint32_t addr, Byte& out) {
    if (addr >= MEM_SIZE) {
        fault(cpu, "address 0x" + hex(addr, 4) + " is outside memory");
        return false;
    }
    out = mem_get(cpu.mem, static_cast<Addr>(addr));
    return true;
}

static bool data_read(CPU& cpu, std::uint32_t addr, Byte& out) { return fetch(cpu, addr, out); }

static bool data_write(CPU& cpu, std::uint32_t addr, Byte v) {
    if (addr >= MEM_SIZE) {
        fault(cpu, "address 0x" + hex(addr, 4) + " is outside memory");
        return false;
    }
    mem_set(cpu.mem, static_cast<Addr>(addr), v);
    return true;
}


bool step(CPU& cpu) {
    if (cpu.halted) return false;

    const Addr pc_at = cpu.pc;
    Byte op = 0;
    if (!fetch(cpu, pc_at, op)) return false;

    std::uint32_t next = static_cast<std::uint32_t>(pc_at) + 1;  

    auto imm8 = [&](Byte& v) {
        if (!fetch(cpu, next, v)) return false;
        next += 1;
        return true;
    };
    auto imm16 = [&](Addr& v) {
        Byte lo = 0, hi = 0;
        if (!fetch(cpu, next, lo) || !fetch(cpu, next + 1, hi)) return false;
        v = static_cast<Addr>(lo | (hi << 8));  
        next += 2;
        return true;
    };

    switch (op) {
        
        case 0x00:  
            cpu.halted = true;
            break;
        case 0x01:  
            break;
        case 0x02:  
            cpu.out += static_cast<char>(cpu.a);
            break;
        case 0x03:  
            cpu.out += std::to_string(cpu.a) + " ";
            break;

        
        case 0x10: {  
            unsigned s = cpu.a + cpu.b;
            cpu.a = static_cast<Byte>(s);
            set_zn(cpu, cpu.a);
            cpu.c = s > 0xFF;
            break;
        }
        case 0x11: {  
            cpu.c = cpu.a < cpu.b;
            cpu.a = static_cast<Byte>(cpu.a - cpu.b);
            set_zn(cpu, cpu.a);
            break;
        }
        case 0x12:  
            cpu.a &= cpu.b; set_zn(cpu, cpu.a); cpu.c = false; break;
        case 0x13:  
            cpu.a |= cpu.b; set_zn(cpu, cpu.a); cpu.c = false; break;
        case 0x14:  
            cpu.a ^= cpu.b; set_zn(cpu, cpu.a); cpu.c = false; break;
        case 0x15:  
            cpu.a = static_cast<Byte>(~cpu.a); set_zn(cpu, cpu.a); cpu.c = false; break;
        case 0x16:  
            cpu.c = (cpu.a & 0x80) != 0;
            cpu.a = static_cast<Byte>(cpu.a << 1);
            set_zn(cpu, cpu.a);
            break;
        case 0x17:  
            cpu.c = (cpu.a & 0x01) != 0;
            cpu.a = static_cast<Byte>(cpu.a >> 1);
            set_zn(cpu, cpu.a);
            break;
        case 0x18:  
            cpu.a = static_cast<Byte>(cpu.a + 1); set_zn(cpu, cpu.a); break;
        case 0x19:  
            cpu.a = static_cast<Byte>(cpu.a - 1); set_zn(cpu, cpu.a); break;
        case 0x1A: {  
            Byte r = static_cast<Byte>(cpu.a - cpu.b);
            cpu.c = cpu.a < cpu.b;
            set_zn(cpu, r);
            break;
        }

        
        case 0x20: { Byte v; if (!imm8(v)) return false; cpu.a = v; break; }  
        case 0x21: { Byte v; if (!imm8(v)) return false; cpu.b = v; break; }  
        case 0x22: {  
            Addr ad; Byte v;
            if (!imm16(ad) || !data_read(cpu, ad, v)) return false;
            cpu.a = v; break;
        }
        case 0x23: {  
            Addr ad; Byte v;
            if (!imm16(ad) || !data_read(cpu, ad, v)) return false;
            cpu.b = v; break;
        }
        case 0x24: {  
            Addr ad;
            if (!imm16(ad) || !data_write(cpu, ad, cpu.a)) return false;
            break;
        }
        case 0x25: {  
            Addr ad;
            if (!imm16(ad) || !data_write(cpu, ad, cpu.b)) return false;
            break;
        }
        case 0x26: cpu.a = cpu.b; break;  
        case 0x27: cpu.b = cpu.a; break;  
        case 0x28: { Addr v; if (!imm16(v)) return false; cpu.h = v; break; }  
        case 0x29: {  
            Byte v;
            if (!data_read(cpu, cpu.h, v)) return false;
            cpu.a = v; break;
        }
        case 0x2A:  
            if (!data_write(cpu, cpu.h, cpu.a)) return false;
            break;
        case 0x2B: cpu.h = static_cast<Addr>(cpu.h + 1); break;  
        case 0x2C: cpu.h = static_cast<Addr>(cpu.h - 1); break;  
        case 0x2D: cpu.a = static_cast<Byte>(cpu.h & 0xFF); break;  

        
        case 0x30: { Addr t; if (!imm16(t)) return false; next = t; break; }                 
        case 0x31: { Addr t; if (!imm16(t)) return false; if (cpu.z)  next = t; break; }    
        case 0x32: { Addr t; if (!imm16(t)) return false; if (!cpu.z) next = t; break; }    
        case 0x33: { Addr t; if (!imm16(t)) return false; if (cpu.c)  next = t; break; }    
        case 0x34: { Addr t; if (!imm16(t)) return false; if (!cpu.c) next = t; break; }    

        default:
            fault(cpu, "unknown opcode 0x" + hex(op, 2) + " at PC=0x" + hex(pc_at, 4));
            return false;
    }

    cpu.pc = static_cast<Addr>(next);
    cpu.steps += 1;
    return true;
}



static std::string disasm(const CPU& cpu, Addr pc, int& len) {
    auto m = [&](std::uint32_t a) -> unsigned { return a < MEM_SIZE ? cpu.mem.bytes[a] : 0u; };
    auto a16 = [&] { return "0x" + hex(m(pc + 1) | (m(pc + 2) << 8), 4); };
    auto a8 = [&] { return "0x" + hex(m(pc + 1), 2); };
    len = 1;
    switch (m(pc)) {
        case 0x00: return "HALT";
        case 0x01: return "NOP";
        case 0x02: return "OUT";
        case 0x03: return "OUTN";
        case 0x10: return "ADD A,B";
        case 0x11: return "SUB A,B";
        case 0x12: return "AND A,B";
        case 0x13: return "OR A,B";
        case 0x14: return "XOR A,B";
        case 0x15: return "NOT A";
        case 0x16: return "SHL A";
        case 0x17: return "SHR A";
        case 0x18: return "INC A";
        case 0x19: return "DEC A";
        case 0x1A: return "CMP A,B";
        case 0x20: len = 2; return "LOADI A," + a8();
        case 0x21: len = 2; return "LOADI B," + a8();
        case 0x22: len = 3; return "LOAD A,[" + a16() + "]";
        case 0x23: len = 3; return "LOAD B,[" + a16() + "]";
        case 0x24: len = 3; return "STORE [" + a16() + "],A";
        case 0x25: len = 3; return "STORE [" + a16() + "],B";
        case 0x26: return "MOV A,B";
        case 0x27: return "MOV B,A";
        case 0x28: len = 3; return "LOADH H," + a16();
        case 0x29: return "LOAD A,[H]";
        case 0x2A: return "STORE [H],A";
        case 0x2B: return "INCH";
        case 0x2C: return "DECH";
        case 0x2D: return "HLOW";
        case 0x30: len = 3; return "JMP " + a16();
        case 0x31: len = 3; return "JZ " + a16();
        case 0x32: len = 3; return "JNZ " + a16();
        case 0x33: len = 3; return "JC " + a16();
        case 0x34: len = 3; return "JNC " + a16();
        default: return "??? 0x" + hex(m(pc), 2);
    }
}



enum class RunResult { Halted, Fault, StepLimit, Breakpoint };

static void print_trace_header() {
    std::cout << " #   PC    інструкція         A   B   Z N C   наступний PC   вивід\n";
}



RunResult run(CPU& cpu, unsigned long limit = STEP_LIMIT,
              std::optional<Addr> breakpoint = std::nullopt, bool trace = false) {
    if (trace) print_trace_header();
    for (unsigned long i = 0; i < limit; ++i) {
        if (cpu.halted) break;
        if (breakpoint && i > 0 && cpu.pc == *breakpoint) return RunResult::Breakpoint;

        const Addr pc_before = cpu.pc;
        const std::size_t out_before = cpu.out.size();
        int len = 1;
        const std::string text = disasm(cpu, pc_before, len);
        const bool ok = step(cpu);

        if (trace && ok) {
            std::cout << std::setw(2) << cpu.steps << "   " << hex(pc_before, 4) << "  "
                      << std::left << std::setw(18) << text << std::right
                      << std::setw(3) << static_cast<unsigned>(cpu.a) << " "
                      << std::setw(3) << static_cast<unsigned>(cpu.b) << "   "
                      << cpu.z << " " << cpu.n << " " << cpu.c << "   "
                      << std::left << std::setw(14) << hex(cpu.pc, 4) << std::right << "  "
                      << cpu.out.substr(out_before) << "\n";
        }
    }
    if (cpu.halted) return cpu.error.empty() ? RunResult::Halted : RunResult::Fault;
    fault(cpu, "step limit (" + std::to_string(limit) + ") exceeded, PC=0x" + hex(cpu.pc, 4));
    return RunResult::StepLimit;
}




std::uint16_t find(const Memory& mem, std::uint16_t lo, std::uint16_t hi, Byte needle) {
    for (std::uint16_t i = lo; i < hi; ++i) {
        if (mem_get(mem, i) == needle) return i;
    }
    return hi;
}



static bool parse_num(const std::string& s, unsigned long& v) {
    char* end = nullptr;
    v = std::strtoul(s.c_str(), &end, 0);
    return !s.empty() && *end == '\0';
}

static void flush_out(CPU& cpu) {
    if (!cpu.out.empty()) {
        std::cout << cpu.out << "\n";
        cpu.out.clear();
    }
}

static void report(CPU& cpu, RunResult r) {
    flush_out(cpu);
    switch (r) {
        case RunResult::Halted: std::cout << "halted after " << cpu.steps << " steps\n"; break;
        case RunResult::Breakpoint: std::cout << "breakpoint at 0x" << hex(cpu.pc, 4) << "\n"; break;
        case RunResult::Fault:
        case RunResult::StepLimit: std::cout << "error: " << cpu.error << "\n"; break;
    }
}

static void print_regs(const CPU& cpu) {
    std::cout << "A=" << hex(cpu.a, 2) << " B=" << hex(cpu.b, 2) << " H=" << hex(cpu.h, 4)
              << " PC=" << hex(cpu.pc, 4) << " SP=" << hex(cpu.sp, 4)
              << " Z=" << cpu.z << " N=" << cpu.n << " C=" << cpu.c
              << (cpu.halted ? "  [halted]" : "") << "\n";
}

static void help() {
    std::cout << "set <addr> <byte>...   покласти байти\n"
                 "dump <lo> <hi>         показати пам'ять [lo, hi)\n"
                 "regs                   регістри й прапорці\n"
                 "step [n]               виконати 1 (або n) інструкцій з трасуванням\n"
                 "run                    виконати до HALT (ліміт " << STEP_LIMIT << " кроків)\n"
                 "trace on|off           трасування під час run\n"
                 "b <addr>               виконати до PC == addr\n"
                 "find <lo> <hi> <byte>  лінійний пошук (C++)\n"
                 "pc <addr>              поставити PC\n"
                 "reset                  скинути процесор (пам'ять лишається)\n"
                 "wipe                   занулити пам'ять і скинути процесор\n"
                 "quit\n";
}

int main() {
    CPU cpu;
    bool trace = false;
    std::string line;
    while (std::cout << "> " << std::flush, std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        if (!(in >> cmd)) continue;
        std::vector<std::string> args;
        for (std::string t; in >> t;) args.push_back(t);
        auto num = [&](std::size_t i, unsigned long& v) {
            return i < args.size() && parse_num(args[i], v);
        };
        unsigned long x = 0, y = 0, w = 0;

        if (cmd == "quit" || cmd == "q") {
            break;
        } else if (cmd == "help") {
            help();
        } else if (cmd == "set") {
            if (!num(0, x) || args.size() < 2 || x >= MEM_SIZE) { std::cout << "usage: set <addr> <byte>...\n"; continue; }
            bool ok = true;
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (!parse_num(args[i], y) || y > 0xFF || x + i - 1 >= MEM_SIZE) { ok = false; break; }
                mem_set(cpu.mem, static_cast<Addr>(x + i - 1), static_cast<Byte>(y));
            }
            if (!ok) std::cout << "error: bad byte or address out of range\n";
        } else if (cmd == "dump") {
            if (!num(0, x) || !num(1, y) || x > y || y > MEM_SIZE) { std::cout << "usage: dump <lo> <hi>\n"; continue; }
            for (unsigned long a = x; a < y; a += 16) {
                std::cout << hex(static_cast<unsigned>(a), 4) << " ";
                for (unsigned long k = a; k < a + 16 && k < y; ++k)
                    std::cout << " " << hex(mem_get(cpu.mem, static_cast<Addr>(k)), 2);
                std::cout << "\n";
            }
        } else if (cmd == "regs") {
            print_regs(cpu);
        } else if (cmd == "trace") {
            if (!args.empty() && (args[0] == "on" || args[0] == "off")) trace = (args[0] == "on");
            else std::cout << "usage: trace on|off\n";
        } else if (cmd == "step") {
            unsigned long n = 1;
            if (!args.empty() && !parse_num(args[0], n)) { std::cout << "usage: step [n]\n"; continue; }
            RunResult r = run(cpu, n, std::nullopt, true);
            flush_out(cpu);
            if (r == RunResult::Fault) std::cout << "error: " << cpu.error << "\n";
            else if (r == RunResult::StepLimit) { cpu.halted = false; cpu.error.clear(); }
            else if (r == RunResult::Halted && cpu.halted) std::cout << "halted\n";
        } else if (cmd == "run") {
            report(cpu, run(cpu, STEP_LIMIT, std::nullopt, trace));
        } else if (cmd == "b") {
            if (!num(0, x) || x >= MEM_SIZE) { std::cout << "usage: b <addr>\n"; continue; }
            report(cpu, run(cpu, STEP_LIMIT, static_cast<Addr>(x), trace));
        } else if (cmd == "find") {
            if (!num(0, x) || !num(1, y) || !num(2, w) || x > y || y > MEM_SIZE || w > 0xFF) {
                std::cout << "usage: find <lo> <hi> <byte>\n"; continue;
            }
            Addr r = find(cpu.mem, static_cast<Addr>(x), static_cast<Addr>(y), static_cast<Byte>(w));
            if (r == y) std::cout << "not found\n";
            else std::cout << "found at 0x" << hex(r, 4) << "\n";
        } else if (cmd == "pc") {
            if (!num(0, x) || x >= MEM_SIZE) { std::cout << "usage: pc <addr>\n"; continue; }
            cpu.pc = static_cast<Addr>(x);
        } else if (cmd == "reset" || cmd == "wipe") {
            Memory keep = cpu.mem;
            cpu = CPU{};
            if (cmd == "reset") cpu.mem = keep;
        } else {
            std::cout << "unknown command '" << cmd << "' (help)\n";
        }
    }
    return 0;
}
