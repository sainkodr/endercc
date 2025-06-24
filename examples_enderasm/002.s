/* 002.s - Fibonacci Sequence */
extern func fibonacci
eter a, b

fibonacci:
  mov a, 1
  mov b, 1
fibonacci_loop:
  tellraw "@a", a // "tellraw a" to write "a" in the tellraw format
  add b, a
  xchg a, b // swap the values of two eternal variables
  jne fibonacci_loop, b, 4181 // if (b != 4181) goto fibonacci_loop;
  ret

