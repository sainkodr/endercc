/* 000.s - Hello, EnderASM! */
extern func load // mark the function as "used somewhere outside this source file"

load: // generates load.mcfunction
  cmd `tellraw @a "Hello, EnderASM!"` // '\n' is added automatically
  ret // <= for readability, enderasm ends the function instead of generating return command

