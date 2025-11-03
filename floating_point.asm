; prog3.asm –  π ≈ 3.1416,  r = 5.0  →  area = π*r²
PUSHF 3.1416
PUSHF 5.0
DUP
MULF          ; r*r
MULF          ; π*r²
PRINT
HALT