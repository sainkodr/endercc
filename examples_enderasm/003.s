/* 003.s - Function calls */
extern func call_me
eter x

call_me:
  mov x, 5
  call print_x
  mov x, 9
  call print_x
  call "nmsp:foreign" // call an mcfunction by signature
  // unfortunately, EnderASM does NOT check if the signature is valid or not
  ret

print_x:
  cmd `tellraw @a [`, tellraw x, `]`
  ret

