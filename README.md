# 🚀 SISA: Soumya Instruction Set Architecture a stack based virtual machine entirely created on C
[![Stars](https://img.shields.io/github/stars/soumyapriyagoswami/SISA-VM?style=social)](https://github.com/soumyapriyagoswami/SISA-VM)
[![Forks](https://img.shields.io/github/forks/soumyapriyagoswami/SISA-VM?style=social)](https://github.com/soumyapriyagoswami/SISA-VM)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C/C++ CI](https://github.com/soumyapriyagoswami/SISA-VM/actions/workflows/ci.yml/badge.svg)](https://github.com/soumyapriyagoswami/SISA-VM/actions)

A **stack-based virtual machine** with integer/float arithmetic, memory access, and control flow—built in **<1000 lines of C** by a 3rd-year B.Tech IT student. Perfect for prototyping languages, teaching CS, or just geeking out on ISAs. No bloat, pure speed. 💻

## Why SISA? (The "Why Another VM?" Story)
- **Student-Built Brilliance**: I (Soumyapriya Goswami) created this during my B.Tech at Kalyani Govt Engineering College to explore VM design. It's Turing-complete, cross-platform (Win/Linux), and outperforms toy VMs in simplicity.
- **Key Wins**: Tagged values (int32/double), two-pass assembler, tracing mode. Handles recursion, loops, and floats out-of-box.
- **Viral Potential**: Like those late-night side projects that hit 4k stars—raw, useful, and community-ready.<grok-card data-id="0b0134" data-type="citation_card"></grok-card>

> "From zero to factorial in 10 ASM lines. Because every CS student deserves their own CPU." – Me

## Quick Start: Run in 30 Seconds
1. **Clone & Build**:
   ```bash
   git clone https://github.com/soumyapriyagoswami/SISA-VM.git
   cd SISA-VM
   cd source_code
   gcc -O2 -std=c11 vm.c -o sisa-vm  # One command, no deps!
   ```
2. **Test with Sample (e.g., compute 5!)**
    ```
    ./vm factorial.asm
    ```
## Features at a Glance

| Feature | Description | Cool Factor |
|---------|-------------|-------------|
| **Arithmetic** | `ADD SUB MUL DIV MOD` (int)  <br> `PUSHF ADDF MULF` (float) | Full int + float in **< 1000 LOC** |
| **Stack & Memory** | `DUP POP` <br> `LOAD STORE` (int‑only store, 4 KB RAM) | Tagged values – no type errors |
| **Control Flow** | `JMP JZ CALL RET` + **label relocation** | Recursive functions & loops |
| **Extras** | Smart `PRINT`, runtime checks, Windows‑safe (`strtok_r`, `strndup`) | No segfaults, friendly errors |
| **Performance** | Pure C99, **< 1 MB** binary, `-O2` ready | Faster than most toy VMs |

---

## Samples to Fork & Play  

<details>
<summary><b>factorial.asm</b> – Recursive `fact(5) → 120`</summary>

```asm
PUSH 5
CALL fact
PRINT
HALT

fact:
    DUP
    PUSH 1
    JZ  base
    DUP
    PUSH 1
    SUB
    CALL fact
    MUL
    RET
base:
    POP
    PUSH 1
    RET
```

## 🧩 Example Programs

### 🟢 **circle.asm** – Compute Circle Area (`π × r²`) with Floats

```asm
PUSHF 3.1415926535
PUSHF 5.0
DUP
MULF          ; r * r
MULF          ; π * r²
PRINT
HALT
```

### 🔵 **array_sum.asm** – Memory + Loop Example

```asm
; store 1..4
PUSH 1  PUSH 0  STORE
PUSH 2  PUSH 1  STORE
PUSH 3  PUSH 2  STORE
PUSH 4  PUSH 3  STORE

PUSH 0          ; sum = 0
PUSH 0          ; i   = 0
loop:
    DUP
    PUSH 4
    JZ  done
    DUP
    LOAD        ; mem[i]
    SWAP
    ADD         ; sum += mem[i]
    INC         ; i++
    JMP loop
done:
    POP
    PRINT
    HALT
```

👉 Run any program with:

```bash
./vm name.asm
```

---

## 🧠 Instruction Highlights

| Type                 | Examples                                      | Description            |
| -------------------- | --------------------------------------------- | ---------------------- |
| **Arithmetic**       | `ADD`, `SUB`, `MUL`, `DIV`                    | Integer ops            |
| **Float Arithmetic** | `ADDF`, `SUBF`, `MULF`, `DIVF`                | Floating-point ops     |
| **Logic & Control**  | `CMP`, `JL`, `JE`, `JMP`, `JZ`                | Comparison & branching |
| **Memory**           | `PUSH`, `POP`, `STORE`, `LOAD`, `DUP`, `SWAP` | Stack & memory         |
| **I/O**              | `PRINT`                                       | Output top of stack    |
| **Flow**             | `HALT`                                        | End program            |

---

## 🧪 Samples Directory

All demo programs live in `/Examples/`.

You can **clone, edit, and run** them to learn how the VM works:

```bash
git clone https://github.com/<you>/SISA-VM.git
cd SISA-VM
./sisa-vm samples/circle.asm
```

PRs are **welcome!** ✨
Try adding new instructions like:

* `SUBF` (Floating subtract)
* `DIVF` (Floating divide)
* Or even your own creative demos!

---

## 🛣️ Roadmap — *Help Build SISA v2*

| Milestone              | Description                             | Status     |
| ---------------------- | --------------------------------------- | ---------- |
| **Float store / cast** | `STOREF`, `INTF`, `FLTI` conversions    | 🧩 Planned |
| **Syscalls**           | `PRINT_STR`, `READ_INT`, `RAND`         | 🧩 Planned |
| **JIT Compilation**    | x86-64 codegen → native execution speed | 💭 Dream   |
| **Your Idea?**         | Open an issue!                          | 🌟 Open    |

---

## 🤝 Contributing

### Fork → Clone

```bash
git clone https://github.com/soumyapriyagoswami/SISA-VM.git
```

### Create a Branch

```bash
git checkout -b feature/subf
```

### Hack, Test & Commit

```bash
git commit -m "Add SUBF/DIVF instruction"
git push origin feature/subf
```

Then open a **Pull Request** — reviews are fast and friendly!
See `CONTRIBUTING.md` for coding style and test harness.

---

## 💬 Code of Conduct

Be **kind**, **curious**, and **collaborative**.
Credit sources, share learnings, and **have fun building systems from scratch**.

---

## ⚖️ License & Credits

**License:** MIT — free to use, modify, and distribute.
**Inspiration:** Forth VMs and student projects like *eLang*.

Built with ❤️ by **Soumyapriya Goswami**
🎓 *B.Tech IT ’27 – Kalyani Government Engineering College*

📧 Email: *[soumyapriya_goswami@example.com](mailto:soumyapriya.goswami.it2023@kgec.ac.in)*


---

> *"SISA is more than a VM — it’s a sandbox for your imagination.
> Write instructions. Execute logic. Build your own universe of computation."*

    
