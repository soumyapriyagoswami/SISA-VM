; Demonstrate dynamic LOAD/STORE
PUSH 42      ; value
PUSH 5       ; address
STORE        ; memory[5] = 42

PUSH 5       ; address
LOAD         ; push memory[5]
PRINT

HALT
