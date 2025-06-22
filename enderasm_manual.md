# EnderASM Manual

TODO: well... write the manual

## Quick Reference

```
M - eternal variable; C - constant expression; S - string; L - label

L:
cmd  M/C/S, M/C/S, ... M/C/S
mov  M, M/C
add  M, M/C
sub  M, M/C
mul  M, M/C
div  M, M/C
mod  M, M/C
min  M, M/C
max  M, M/C
xchg M, M
jmp  L
jl   L, M/C, M/C
jle  L, M/C, M/C
jg   L, M/C, M/C
jge  L, M/C, M/C
je   L, M/C, M/C
jne  L, M/C, M/C
call L/S
ret
unreachable
eter M0, M1, M2, ... MN
extern eter M0, M1, M2, ... MN
extern func L0, L1, L2, ... LN
enum { C0, C1 = C, C2, ... CN }
const C0 = C
```

