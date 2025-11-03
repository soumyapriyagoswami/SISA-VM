; factorial.asm
PUSH 5
CALL factorial
PRINT
HALT

factorial:
    DUP
    JZ base
    DUP
    PUSH 1
    SUB
    CALL factorial
    MUL
    RET
base:
    POP
    PUSH 1
    RET
