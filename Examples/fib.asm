; fib.asm - Fibonacci with float (just for fun)
main:
    PUSHF 1.0
    PUSHF 1.0
    PUSH 20
loop:
    DUP
    JZ done
    PRINT           ; print counter
    ADDF            ; a + b
    ROT             ; c b a → b a c
    SWAP            ; b c a
    DEC
    JMP loop
done:
    PRINT           ; print last fib
    HALT