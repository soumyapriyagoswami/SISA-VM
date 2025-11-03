; prog4.asm – store 42 at address 10, load it back, add 8, print
PUSH 10       ; address
PUSH 42       ; value
STORE         ; mem[10] = 42
PUSH 10
LOAD          ; push mem[10]
PUSH 8
ADD
PRINT
HALT