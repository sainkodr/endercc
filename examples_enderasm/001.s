/* 001.s - Print a message 5 times */
extern func print_x5
eter x // you must declare eternal variables before using them

print_x5:
  mov x, 0
loop:
  cmd `tellraw @a "This message is printed 5 times!"`
  add x, 1
  jl loop, x, 5 // if (x <= 5) goto loop;
  ret
// it's nice to have loops, but remember you are limited to 65536 commads per tick by default

