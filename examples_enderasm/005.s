/* 005.s - Do it yourself! */
eter a, b
extern func some_function
some_function:
  mov a, 5
  mov b, 9
  cmd `execute if score `, a, ` < `, b, ` run say @a Hi!`
  sub b, 4
  cmd `execute if score `, a, ` < `, b, ` run say @a Hi?`
  cmd `return fail` // your own return command!
  unreachable // tell EnderASM, that you terminated the function yourself

