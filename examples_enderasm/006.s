/* 006.s - neg, abs, not, inc, dec */
extern func test_unary
eter x, not_x

test_unary:
  
  mov x, 10
  neg x
  jne fail, x, -10
  tellraw "@a", "neg OK"

  mov x, 42
  abs x
  jne fail, x, 42
  tellraw "@a", "abs OK"
  
  mov x, -5
  abs x
  jne fail, x, 5
  tellraw "@a", "abs OK"

  mov x, 99
  inc x
  jne fail, x, 100
  tellraw "@a", "inc OK"
  
  mov x, 0
  dec x
  jne fail, x, -1
  tellraw "@a", "dec OK"
  
  mov x, 240
  mov not_x, x
  not not_x
  neg x
  dec x
  jne fail, x, not_x
  tellraw "@a", "not OK"

  tellraw "@a", "DONE!"
  ret
fail:
  tellraw "@a", "Something wrong..."
  ret

