# Command-line options

TODO: well... write the manual

# Instructions

## "cmd" Pseudo-instruction

Constructs a Minecraft command from a comma-separated list of elements, where each element can be
a quoted text, a constant, a label, or an eternal variable. `cmd` adds a newline character at the
end of the constructed command automatically.

By default, eternal variables are referenced in the format `nickname objective`, but for commands
like `tellraw` you need a different format. You can change the format by adding the corresponding
keyword before the name of the variable. For example, `tellraw x` references the variable in the
format `{"score":{"name":"x","objective":"INT"}}`.

Currently, EnderASM Assembler does NOT analyze the constructed command. That is, if you
constructing a `return` command, the assembler will NOT treat it as an end of the code sequence.
To fix this, you should add the `unreachable` pseudo-instruction after the `cmd`, to tell the
assembler that the constructed command was the last command in the code sequence.

### Examples

```c
  cmd `tellraw @a "text"`
// => tellraw @a "text"
  extern eter x
  cmd `scoreboard players set `, x, ` 0`
// => scoreboard players set x INT 0
  eter y
  cmd `scoreboard players set `, y, ` 0`
// => scoreboard players set _0001 INT 0
  extern eter z
  cmd `tellraw @a [`, tellraw z, `]`
// => tellraw @a [{"score":{"name":"z","objective":"INT"}}]
  const COUNT = 10
  cmd `say @a `, COUNT
// => say @a 10
infloop:
  cmd `funcion `, infloop
// => funcion nmsp:infloop
foo:
  cmd `return fail`
  unreachable // now the assembler knows that buz is NOT a part of foo.
buz:
  // ...
```

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

