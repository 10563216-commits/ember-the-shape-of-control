#!/usr/bin/env bash
set -u
fail=0
check() {  # check <назва> <очікуваний фрагмент> <команди для ember>
  out=$(printf "$3" | ./ember 2>&1)
  if grep -qF -- "$2" <<<"$out"; then echo "ok   $1"; else echo "FAIL $1 (очікував: $2)"; echo "$out"; fail=1; fi
}
FIND='set 0 0x28 0 8 0x29 0x21 0x41 0x1A 0x31 0x19 0 0x2D 0x21 7 0x1A 0x31 0x15 0 0x2B 0x30 3 0 0x20 0x2D 0x02 0 0x2D 0x03 0'
check "відлік 3 2 1"        "3 2 1 "                    'set 0 0x20 3 0x03 0x19 0x32 2 0 0\nrun\n'
check "відлік: 11 кроків"   "halted after 11 steps"     'set 0 0x20 3 0x03 0x19 0x32 2 0 0\nrun\n'
check "невідомий опкод"     "unknown opcode 0xFF at PC=0x0000" 'set 0 0xFF\nrun\n'
check "ліміт кроків"        "step limit (100000) exceeded"     'set 0 0x30 0 0\nrun\n'
check "JMP не додає розмір" "PC=0005"                   'set 0 0x30 5 0 0 0 0 0\nstep\nregs\n'
check "JZ не спрацював"     "PC=0003"                   'set 0 0x31 9 0\nstep\nregs\n'
check "JZ спрацював"        "PC=0009"                   'set 0 0x20 1 0x19 0x31 9 0\nstep 3\nregs\n'
check "LOADI не чіпає Z"    "Z=0"                       'set 0 0x20 0\nstep\nregs\n'
check "find C++ знайшов"    "found at 0x0803"           'set 0x800 1 2 3 0x41\nfind 0x800 0x808 0x41\n'
check "find C++ промах"     "not found"                 'set 0x800 1 2 3 4\nfind 0x800 0x808 0x41\n'
check "ember-пошук: 3"      "3 "                        "$FIND\nset 0x800 0x10 0x20 0x30 0x41\nrun\n"
check "ember-пошук: 0"      "0 "                        "$FIND\nset 0x800 0x41\nrun\n"
check "ember-пошук: 7"      "7 "                        "$FIND\nset 0x800 1 2 3 4 5 6 7 0x41\nrun\n"
check "ember-пошук: промах" "-"                         "$FIND\nset 0x800 1 2 3 4 5 6 7 8\nrun\n"
check "breakpoint"          "breakpoint at 0x0007"      'set 0 0x20 3 0x03 0x19 0x32 2 0 0\nb 7\n'
exit $fail
