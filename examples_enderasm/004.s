/* 004.s - Constant expressions! */
enum { // enumerations like in C
  X = 3,
  Y, // == 4
  Z  // == 5
}

const MY_NUMBER = 42 // to define a standalone constant

extern func main

main:
  cmd `say @a `, X * 2
  cmd `say @a `, Y + Z
  cmd `say @a `, MY_NUMBER
  ret

