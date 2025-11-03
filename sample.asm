; sample.asm - prints 3 2 1
PUSH 3
loop:
    DUP
    JZ end
    DUP
    PRINT
    PUSH 1
    SUB
    JMP loop
end:
    HALT
