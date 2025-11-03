// vm.c - SoumyaVM extended arithmetic + floating point
// Build: gcc -O2 -std=c11 vm.c -o vm
// Usage: ./vm <program.asm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
// Windows-safe strndup
static char *strndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

// Windows-safe strtok_r
static char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;
    if (str)
        *saveptr = str;
    if (!*saveptr)
        return NULL;
    token = *saveptr + strspn(*saveptr, delim);
    if (*token == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    *saveptr = token + strcspn(token, delim);
    if (**saveptr != '\0') {
        **saveptr = '\0';
        (*saveptr)++;
    } else {
        *saveptr = NULL;
    }
    return token;
}
#endif
#ifdef __linux__
    #include <sys/mman.h>
#elif defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

#define STACK_SIZE 1024
#define CODE_CAP   131072
#define MEM_SIZE   4096
#define MAX_LABELS 2048
#define MAX_REFS   2048
#define LINE_MAX   512

// Opcodes
enum {
    OP_NOP   = 0x00,
    OP_PUSH  = 0x01, // int32 immediate (4 bytes)
    OP_PUSHF = 0x02, // double immediate (8 bytes)
    OP_ADD   = 0x03, // int add
    OP_SUB   = 0x04,
    OP_MUL   = 0x05,
    OP_DIV   = 0x06, // integer division
    OP_MOD   = 0x07,
    OP_INC   = 0x08, // increment top (int)
    OP_DEC   = 0x09, // decrement top (int)
    OP_NEG   = 0x0A, // negate top (int)
    OP_ADDF  = 0x0B, // float add
    OP_MULF  = 0x0C, // float mul
    OP_DUP   = 0x0D,
    OP_PRINT = 0x0E, // smart print (int or float)
    OP_POP   = 0x0F,
    OP_LOAD  = 0x10, // dynamic addr (pop addr -> push mem[addr] as int)
    OP_STORE = 0x11, // dynamic addr (pop addr; pop val; mem[addr]=val) stores int
    OP_JMP   = 0x12, // u32 target
    OP_JZ    = 0x13, // u32 target (pop top; if zero jump) - only checks integers (zero int) or floats with value==0.0
    OP_CALL  = 0x14, // u32 target
    OP_RET   = 0x15,
    OP_HALT  = 0xFF
};

// Value type tagging
typedef enum { TY_INT = 1, TY_FLOAT = 2 } ValType;
typedef struct {
    ValType type;
    union {
        int32_t i;
        double  f;
    } v;
} Value;

// VM state
static unsigned char *codebuf = NULL;
static size_t code_len = 0;
static Value stack[STACK_SIZE];
static int sp = 0;
static uint32_t callstack[STACK_SIZE];
static int csp = 0;
static int32_t memory_arr[MEM_SIZE];
static size_t ip = 0;

// Labels / relocations
typedef struct { char name[128]; uint32_t offset; } Label;
typedef struct { char name[128]; size_t patch_pos; } Reloc;
static Label labels[MAX_LABELS];
static int label_count = 0;
static Reloc relocs[MAX_REFS];
static int reloc_count = 0;

// Helpers
static void runtime_err(const char *msg) { fprintf(stderr, "Runtime error: %s\n", msg); exit(1); }

static void push_int(int32_t x) {
    if (sp >= STACK_SIZE) runtime_err("stack overflow");
    stack[sp].type = TY_INT;
    stack[sp].v.i = x;
    sp++;
}
static void push_float(double f) {
    if (sp >= STACK_SIZE) runtime_err("stack overflow");
    stack[sp].type = TY_FLOAT;
    stack[sp].v.f = f;
    sp++;
}
static Value pop_val() {
    if (sp <= 0) runtime_err("stack underflow");
    return stack[--sp];
}
static Value peek_val() {
    if (sp <= 0) runtime_err("stack underflow (peek)");
    return stack[sp-1];
}
static int32_t pop_int_checked(const char *opname) {
    Value v = pop_val();
    if (v.type != TY_INT) { fprintf(stderr, "%s expects integer on stack\n", opname); exit(1); }
    return v.v.i;
}
static double pop_float_checked(const char *opname) {
    Value v = pop_val();
    if (v.type != TY_FLOAT) { fprintf(stderr, "%s expects float on stack\n", opname); exit(1); }
    return v.v.f;
}
static void push_from_value(Value v) {
    if (sp >= STACK_SIZE) runtime_err("stack overflow");
    stack[sp++] = v;
}

// Label / reloc helpers
static void add_label(const char *name, uint32_t offset) {
    if (label_count >= MAX_LABELS) runtime_err("too many labels");
    strncpy(labels[label_count].name, name, sizeof(labels[0].name)-1);
    labels[label_count].offset = offset;
    label_count++;
}
static int find_label(const char *name) {
    for (int i=0;i<label_count;i++) if (strcmp(labels[i].name, name)==0) return labels[i].offset;
    return -1;
}
static void add_reloc(const char *name, size_t patch_pos) {
    if (reloc_count >= MAX_REFS) runtime_err("too many relocations");
    strncpy(relocs[reloc_count].name, name, sizeof(relocs[0].name)-1);
    relocs[reloc_count].patch_pos = patch_pos;
    reloc_count++;
}

// Bytecode builder
typedef struct {
    unsigned char *buf;
    size_t len;
    size_t cap;
} Builder;
static Builder builder_new(size_t cap) {
    Builder b; b.cap = cap; b.len = 0; b.buf = malloc(cap);
    if (!b.buf) { perror("malloc"); exit(1); } return b;
}
static void b_emit_u8(Builder *b, uint8_t x) {
    if (b->len + 1 > b->cap) runtime_err("bytecode overflow");
    b->buf[b->len++] = x;
}
static void b_emit_i32_le(Builder *b, int32_t x) {
    if (b->len + 4 > b->cap) runtime_err("bytecode overflow");
    for (int i=0;i<4;i++) b->buf[b->len++] = (unsigned char)((x >> (8*i)) & 0xFF);
}
static void b_emit_u32_le(Builder *b, uint32_t x) {
    if (b->len + 4 > b->cap) runtime_err("bytecode overflow");
    for (int i=0;i<4;i++) b->buf[b->len++] = (unsigned char)((x >> (8*i)) & 0xFF);
}
static void b_emit_double_le(Builder *b, double d) {
    if (b->len + 8 > b->cap) runtime_err("bytecode overflow");
    union { double f; uint8_t b[8]; } u;
    u.f = d;
    // little-endian
    for (int i=0;i<8;i++) b->buf[b->len++] = u.b[i];
}
static void b_patch_u32_le(Builder *b, size_t pos, uint32_t x) {
    if (pos + 4 > b->len) runtime_err("patch out of bounds");
    for (int i=0;i<4;i++) b->buf[pos + i] = (unsigned char)((x >> (8*i)) & 0xFF);
}

// Simple trim
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}
static int is_number_token(const char *s) {
    if (!s || !*s) return 0;
    // allow - sign and 0x hex and decimal
    char *p = (char*)s;
    if (*p == '-' || *p == '+') p++;
    if (*p == '0' && (p[1]=='x' || p[1]=='X')) {
        p += 2;
        if (!isxdigit((unsigned char)*p)) return 0;
        for (; *p; ++p) if (!isxdigit((unsigned char)*p)) return 0;
        return 1;
    }
    int seen_digit = 0;
    for (; *p; ++p) {
        if (!isdigit((unsigned char)*p)) return 0;
        seen_digit = 1;
    }
    return seen_digit;
}

// Tokenize by whitespace and comma; returns malloc'd tokens (caller must free)
static int tokenize_line(char *line, char **out, int max_tokens) {
    int n = 0;
    char *p = line;
    while (*p && n < max_tokens) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == ';' || *p == '#') break;
        char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != ',') p++;
        int len = p - start;
        char *tok = malloc(len + 1);
        memcpy(tok, start, len);
        tok[len] = '\0';
        out[n++] = tok;
        if (*p == ',') p++;
    }
    return n;
}
static void free_tokens(char **toks, int n) { for (int i=0;i<n;i++) free(toks[i]); }

// Assembler (two-pass from string)
static unsigned char *assemble_from_string(const char *src, size_t *out_len) {
    Builder b = builder_new(CODE_CAP);
    // We'll parse line by line:
    char *copy = strdup(src);
    char *line = NULL;
    char *saveptr = NULL;
    int lineno = 0;
    line = strtok_r(copy, "\n", &saveptr);
    while (line) {
        lineno++;
        char *ln = trim(line);
        if (!*ln || ln[0] == ';' || ln[0] == '#') { line = strtok_r(NULL, "\n", &saveptr); continue; }
        // label?
        char *colon = strchr(ln, ':');
        if (colon) {
            // label name from ln to colon
            size_t lnlen = colon - ln;
            char *lbl = strndup(ln, lnlen);
            char *lt = trim(lbl);
            if (strlen(lt) == 0) { fprintf(stderr,"Empty label at line %d\n",lineno); exit(1); }
            add_label(lt, (uint32_t)b.len);
            free(lbl);
            // rest after colon
            char *rest = colon + 1; rest = trim(rest);
            if (!*rest) { line = strtok_r(NULL, "\n", &saveptr); continue; }
            ln = rest;
        }
        // tokenize (max 3 tokens)
        char *toks[3] = {0};
        int tn = tokenize_line(ln, toks, 3);
        if (tn == 0) { line = strtok_r(NULL, "\n", &saveptr); continue; }
        // uppercase copy for match
        char cmdu[64]; strncpy(cmdu, toks[0], sizeof(cmdu)-1); cmdu[sizeof(cmdu)-1]=0;
        for (char *p = cmdu; *p; ++p) *p = (char)toupper((unsigned char)*p);

        if (strcmp(cmdu, "PUSH") == 0) {
            if (tn < 2) { fprintf(stderr,"PUSH missing arg at line %d\n",lineno); exit(1); }
            int32_t v = (int32_t)strtol(toks[1], NULL, 0);
            b_emit_u8(&b, OP_PUSH);
            b_emit_i32_le(&b, v);
        } else if (strcmp(cmdu, "PUSHF") == 0) {
            if (tn < 2) { fprintf(stderr,"PUSHF missing arg at line %d\n",lineno); exit(1); }
            char *endptr = NULL;
            double dv = strtod(toks[1], &endptr);
            if (endptr == toks[1]) { fprintf(stderr,"Invalid float literal '%s' at line %d\n", toks[1], lineno); exit(1); }
            b_emit_u8(&b, OP_PUSHF);
            b_emit_double_le(&b, dv);
        } else if (strcmp(cmdu, "ADD") == 0) { b_emit_u8(&b, OP_ADD); }
        else if (strcmp(cmdu, "SUB") == 0) { b_emit_u8(&b, OP_SUB); }
        else if (strcmp(cmdu, "MUL") == 0) { b_emit_u8(&b, OP_MUL); }
        else if (strcmp(cmdu, "DIV") == 0) { b_emit_u8(&b, OP_DIV); }
        else if (strcmp(cmdu, "MOD") == 0) { b_emit_u8(&b, OP_MOD); }
        else if (strcmp(cmdu, "INC") == 0) { b_emit_u8(&b, OP_INC); }
        else if (strcmp(cmdu, "DEC") == 0) { b_emit_u8(&b, OP_DEC); }
        else if (strcmp(cmdu, "NEG") == 0) { b_emit_u8(&b, OP_NEG); }
        else if (strcmp(cmdu, "ADDF") == 0) { b_emit_u8(&b, OP_ADDF); }
        else if (strcmp(cmdu, "MULF") == 0) { b_emit_u8(&b, OP_MULF); }
        else if (strcmp(cmdu, "DUP") == 0) { b_emit_u8(&b, OP_DUP); }
        else if (strcmp(cmdu, "PRINT") == 0) { b_emit_u8(&b, OP_PRINT); }
        else if (strcmp(cmdu, "POP") == 0) { b_emit_u8(&b, OP_POP); }
        else if (strcmp(cmdu, "LOAD") == 0) { b_emit_u8(&b, OP_LOAD); }
        else if (strcmp(cmdu, "STORE") == 0) { b_emit_u8(&b, OP_STORE); }
        else if (strcmp(cmdu, "JMP")==0 || strcmp(cmdu, "JZ")==0 || strcmp(cmdu, "CALL")==0) {
            unsigned char op = (strcmp(cmdu,"JMP")==0)?OP_JMP:(strcmp(cmdu,"JZ")==0?OP_JZ:OP_CALL);
            b_emit_u8(&b, op);
            if (tn < 2) { fprintf(stderr,"%s missing target at line %d\n", cmdu, lineno); exit(1); }
            // if numeric target given, accept it as absolute offset
            char *targ = toks[1];
            int isnum = 1;
            char *p = targ; if (*p=='+'||*p=='-') p++;
            if (*p=='0' && (p[1]=='x' || p[1]=='X')) {
                p += 2;
                if (!isxdigit((unsigned char)*p)) isnum = 0;
                for (; *p; ++p) if (!isxdigit((unsigned char)*p)) isnum = 0;
            } else {
                for (; *p; ++p) if (!isdigit((unsigned char)*p)) { isnum = 0; break; }
            }
            if (isnum) {
                uint32_t off = (uint32_t)strtoul(targ, NULL, 0);
                b_emit_u32_le(&b, off);
            } else {
                size_t patch_pos = b.len;
                b_emit_u32_le(&b, 0);
                add_reloc(toks[1], patch_pos);
            }
        } else if (strcmp(cmdu, "RET") == 0) { b_emit_u8(&b, OP_RET); }
        else if (strcmp(cmdu, "HALT") == 0) { b_emit_u8(&b, OP_HALT); }
        else {
            fprintf(stderr, "Unknown instruction '%s' at line %d\n", toks[0], lineno);
            exit(1);
        }

        free_tokens(toks, tn);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    // patch relocations
    for (int r = 0; r < reloc_count; ++r) {
        int tgt = find_label(relocs[r].name);
        if (tgt < 0) { fprintf(stderr, "Undefined label: %s\n", relocs[r].name); exit(1); }
        b_patch_u32_le(&b, relocs[r].patch_pos, (uint32_t)tgt);
    }

    unsigned char *out = malloc(b.len);
    if (!out) runtime_err("malloc failed");
    memcpy(out, b.buf, b.len);
    *out_len = b.len;
    free(b.buf);
    free(copy);
    return out;
}

// Simple TRACE printer
static const char *op_name(unsigned char op) {
    switch(op) {
        case OP_PUSH: return "PUSH";
        case OP_PUSHF: return "PUSHF";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_MOD: return "MOD";
        case OP_INC: return "INC";
        case OP_DEC: return "DEC";
        case OP_NEG: return "NEG";
        case OP_ADDF: return "ADDF";
        case OP_MULF: return "MULF";
        case OP_DUP: return "DUP";
        case OP_PRINT: return "PRINT";
        case OP_POP: return "POP";
        case OP_LOAD: return "LOAD";
        case OP_STORE: return "STORE";
        case OP_JMP: return "JMP";
        case OP_JZ: return "JZ";
        case OP_CALL: return "CALL";
        case OP_RET: return "RET";
        case OP_HALT: return "HALT";
        default: return "UNK";
    }
}

static void print_stack_snapshot() {
    printf(" [stack:");
    int start = sp - 8; if (start < 0) start = 0;
    for (int i = start; i < sp; ++i) {
        if (stack[i].type == TY_INT) printf(" %d", stack[i].v.i);
        else printf(" %g", stack[i].v.f);
    }
    printf(" ]\n");
}

// Execution
static void run_vm() {
    ip = 0;
    while (ip < code_len) {
        size_t cur = ip;
        unsigned char op = codebuf[ip++];
        // TRACE
        printf("TRACE ip=%04zu %-6s", cur, op_name(op));
        // show immediates for some ops
        if (op == OP_PUSH) {
            if (ip + 4 <= code_len) {
                int32_t imm = 0;
                memcpy(&imm, &codebuf[ip], 4);
                printf(" %d", imm);
            }
        } else if (op == OP_PUSHF) {
            if (ip + 8 <= code_len) {
                union { double f; uint8_t b[8]; } u;
                for (int i=0;i<8;i++) u.b[i] = codebuf[ip + i];
                printf(" %g", u.f);
            }
        } else if (op==OP_JMP || op==OP_JZ || op==OP_CALL) {
            if (ip + 4 <= code_len) {
                uint32_t tgt = 0; memcpy(&tgt, &codebuf[ip], 4);
                printf(" %u", tgt);
            }
        }
        print_stack_snapshot();

        switch(op) {
            case OP_NOP: break;
            case OP_PUSH: {
                if (ip + 4 > code_len) runtime_err("truncated PUSH");
                int32_t imm; memcpy(&imm, &codebuf[ip], 4); ip += 4;
                push_int(imm);
                break;
            }
            case OP_PUSHF: {
                if (ip + 8 > code_len) runtime_err("truncated PUSHF");
                union { double f; uint8_t b[8]; } u;
                for (int i=0;i<8;i++) u.b[i] = codebuf[ip + i];
                ip += 8;
                push_float(u.f);
                break;
            }
            case OP_ADD: {
                int32_t a = pop_int_checked("ADD"); int32_t b = pop_int_checked("ADD");
                push_int(b + a);
                break;
            }
            case OP_SUB: {
                int32_t a = pop_int_checked("SUB"); int32_t b = pop_int_checked("SUB");
                push_int(b - a);
                break;
            }
            case OP_MUL: {
                int32_t a = pop_int_checked("MUL"); int32_t b = pop_int_checked("MUL");
                push_int(b * a);
                break;
            }
            case OP_DIV: {
                int32_t a = pop_int_checked("DIV"); int32_t b = pop_int_checked("DIV");
                if (a == 0) runtime_err("division by zero");
                push_int(b / a);
                break;
            }
            case OP_MOD: {
                int32_t a = pop_int_checked("MOD"); int32_t b = pop_int_checked("MOD");
                if (a == 0) runtime_err("modulo by zero");
                push_int(b % a);
                break;
            }
            case OP_INC: {
                Value v = pop_val();
                if (v.type != TY_INT) runtime_err("INC expects integer");
                v.v.i += 1;
                push_from_value(v);
                break;
            }
            case OP_DEC: {
                Value v = pop_val();
                if (v.type != TY_INT) runtime_err("DEC expects integer");
                v.v.i -= 1;
                push_from_value(v);
                break;
            }
            case OP_NEG: {
                Value v = pop_val();
                if (v.type != TY_INT) runtime_err("NEG expects integer");
                v.v.i = -v.v.i;
                push_from_value(v);
                break;
            }
            case OP_ADDF: {
                double a = pop_float_checked("ADDF"); double b = pop_float_checked("ADDF");
                push_float(b + a);
                break;
            }
            case OP_MULF: {
                double a = pop_float_checked("MULF"); double b = pop_float_checked("MULF");
                push_float(b * a);
                break;
            }
            case OP_DUP: {
                Value v = peek_val(); push_from_value(v); break;
            }
            case OP_PRINT: {
                Value v = pop_val();
                if (v.type == TY_INT) printf("%d\n", v.v.i);
                else printf("%g\n", v.v.f);
                break;
            }
            case OP_POP: { pop_val(); break; }
            case OP_LOAD: {
                // pop addr (int) and push memory[addr] as int
                Value a = pop_val();
                if (a.type != TY_INT) runtime_err("LOAD expects integer address");
                int32_t addr = a.v.i;
                if (addr < 0 || addr >= MEM_SIZE) runtime_err("LOAD address out of bounds");
                push_int(memory_arr[addr]);
                break;
            }
            case OP_STORE: {
                // pop addr (int), pop value (int required) and store memory[addr]=value
                Value addrv = pop_val();
                if (addrv.type != TY_INT) runtime_err("STORE expects integer address");
                int32_t addr = addrv.v.i;
                if (addr < 0 || addr >= MEM_SIZE) runtime_err("STORE address out of bounds");
                Value val = pop_val();
                if (val.type != TY_INT) runtime_err("STORE currently supports integers only");
                memory_arr[addr] = val.v.i;
                break;
            }
            case OP_JMP: {
                if (ip + 4 > code_len) runtime_err("truncated JMP");
                uint32_t tgt; memcpy(&tgt, &codebuf[ip], 4); ip = (size_t)tgt;
                break;
            }
            case OP_JZ: {
                if (ip + 4 > code_len) runtime_err("truncated JZ");
                uint32_t tgt; memcpy(&tgt, &codebuf[ip], 4); ip += 4;
                {
                    Value v = pop_val();
                    int is_zero = 0;
                    if (v.type == TY_INT) is_zero = (v.v.i == 0);
                    else is_zero = (v.v.f == 0.0);
                    if (is_zero) ip = (size_t)tgt;
                }
                break;
            }
            case OP_CALL: {
                if (ip + 4 > code_len) runtime_err("truncated CALL");
                uint32_t tgt; memcpy(&tgt, &codebuf[ip], 4); ip += 4;
                if (csp >= STACK_SIZE) runtime_err("call stack overflow");
                callstack[csp++] = (uint32_t)ip;
                ip = (size_t)tgt;
                break;
            }
            case OP_RET: {
                if (csp <= 0) runtime_err("call stack underflow");
                ip = (size_t)callstack[--csp];
                break;
            }
            case OP_HALT: return;
            default:
                fprintf(stderr, "Unknown opcode %02X at %zu\n", op, cur); exit(1);
        }
    }
}

// Read file into string
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    buf[sz] = 0;
    fclose(f);
    return buf;
}

// Entrypoint: assemble & run file
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <program.asm>\n\n", argv[0]);
        printf("Integer sample: sample_int.asm\n");
        printf("Float sample:   sample_float.asm\n");
        return 0;
    }

    char *src = read_file(argv[1]);
    if (!src) { fprintf(stderr, "Failed to open '%s'\n", argv[1]); return 1; }

    unsigned char *bc = assemble_from_string(src, &code_len);
    free(src);
    codebuf = bc;

    printf("Assembled %zu bytes.\n", code_len);
    run_vm();

    free(bc);
    return 0;
}
