// COSMO-32 BASIC Interpreter
// MS-DOS BASIC compatible subset (integer-only)
//
// Statements: PRINT, LET, INPUT, IF/THEN/ELSE, GOTO, GOSUB/RETURN,
//             FOR/TO/STEP/NEXT, WHILE/WEND, DIM, DATA/READ/RESTORE,
//             ON...GOTO/GOSUB, REM, END, STOP
// Graphics:   CLS, PSET x,y,c, LINE x1,y1,x2,y2,c, CIRCLE x,y,r,c,
//             FCIRCLE x,y,r,c, PAINT x,y,fill,border
// Commands:   RUN, LIST, NEW, LOAD, SAVE, BYE
// Operators:  + - * / MOD, = <> < > <= >=, AND OR NOT
// Functions:  ABS INT SGN RND, LEN VAL ASC, CHR$ STR$ LEFT$ RIGHT$ MID$
//             INKEY$ TIMER
// Variables:  Multi-character names (e.g., COUNT, NAME$), arrays with DIM
// Colors:     0=Black 1=Blue 2=Green 3=Cyan 4=Red 5=Magenta 6=Brown
//             7=LightGray 8=DarkGray 9=LightBlue 10=LightGreen 11=LightCyan
//             12=LightRed 13=LightMagenta 14=Yellow 15=White

#include <stdint.h>
#include "const.h"
#include "config.h"

extern void putchar(int c);
extern int getchar(void);
extern int getchar_nonblock(void);  // Returns -1 if no char available
extern uint32_t get_timer_ms(void);
extern int32_t tftp_get(const char *filename);
extern int32_t tftp_put(const char *filename, const char *data, uint32_t size);

// Graphics functions from display.c
extern void display_clear(void);
extern void display_pset(int x, int y, uint8_t color);
extern void display_line(int x0, int y0, int x1, int y1, uint8_t color);
extern void display_circle(int cx, int cy, int r, uint8_t color);
extern void display_fill_circle(int cx, int cy, int r, uint8_t color);
extern void display_paint(int x, int y, uint8_t fill_color, uint8_t border_color);

//----------------------------------------------------------------------
// Configuration
//----------------------------------------------------------------------

#define MAX_LINES       BASIC_MAX_LINES
#define MAX_LINE_LEN    BASIC_MAX_LINE_LEN
#define MAX_STACK       BASIC_MAX_STACK
#define MAX_FOR_DEPTH   BASIC_MAX_FOR_DEPTH
#define MAX_STRING_LEN  64
#define MAX_ARRAY_SIZE  100
#define MAX_DATA_ITEMS  256
#define MAX_VAR_NAME    16
#define MAX_VARIABLES   64

//----------------------------------------------------------------------
// Data structures
//----------------------------------------------------------------------

// Program storage
static char program[MAX_LINES][MAX_LINE_LEN];
static uint16_t line_nums[MAX_LINES];
static int num_lines = 0;

// Variable entry
typedef struct {
    char name[MAX_VAR_NAME];    // Variable name (uppercase)
    int is_string;              // 1 if string variable (ends with $)
    int is_array;               // 1 if array (DIM'd)
    int array_size;             // Size if array
    union {
        int32_t int_val;        // Integer value
        char str_val[MAX_STRING_LEN];  // String value
        int32_t *int_array;     // Integer array pointer
        char (*str_array)[MAX_STRING_LEN];  // String array pointer
    };
} Variable;

static Variable variables[MAX_VARIABLES];
static int num_vars = 0;

// Execution state
static int running = 0;
static int current_line = 0;
static const char *ptr;

// GOSUB stack
static int gosub_stack[MAX_STACK];
static int gosub_sp = 0;

// FOR loop stack
typedef struct {
    int var_idx;            // Index into variables[]
    int32_t limit;
    int32_t step;
    int return_line;
} ForFrame;
static ForFrame for_stack[MAX_FOR_DEPTH];
static int for_sp = 0;

// WHILE stack
static int while_stack[MAX_STACK];
static int while_sp = 0;

// DATA/READ state
static int data_line = 0;
static const char *data_ptr = 0;

// RNG state
static uint32_t rng_state = 12345;

// Forward declarations
static int32_t expr(void);
static void str_expr(char *dest);
static void execute_line(const char *line);

//----------------------------------------------------------------------
// Utility functions
//----------------------------------------------------------------------

static void print_char(char c) { putchar(c); }
static void print_newline(void) { putchar('\n'); }

static void print_string(const char *s) {
    while (*s) putchar(*s++);
}

static void print_int(int32_t n) {
    char buf[12];
    int i = 0;
    int neg = 0;

    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) { putchar('0'); return; }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    if (neg) putchar('-');
    while (i > 0) putchar(buf[--i]);
}

static void skip_spaces(void) {
    while (*ptr == ' ' || *ptr == '\t') ptr++;
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static char to_upper(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static int match_keyword(const char *kw) {
    const char *p = ptr;
    while (*kw) {
        if (to_upper(*p) != *kw) return 0;
        p++; kw++;
    }
    if (is_alpha(*p)) return 0;
    ptr = p;
    skip_spaces();
    return 1;
}

static int32_t parse_number(void) {
    int32_t result = 0;
    int neg = (*ptr == '-');
    if (neg) ptr++;
    while (is_digit(*ptr)) {
        result = result * 10 + (*ptr - '0');
        ptr++;
    }
    return neg ? -result : result;
}

static void str_copy(char *dest, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

static int str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

//----------------------------------------------------------------------
// Variable management
//----------------------------------------------------------------------

// Parse variable name from ptr, store in name buffer (uppercase)
// Returns 1 if string var (ends with $), 0 if integer
static int parse_var_name(char *name) {
    int i = 0;
    int is_string = 0;

    // First char must be alpha
    if (!is_alpha(*ptr)) { name[0] = '\0'; return 0; }

    // Read alphanumeric chars
    while ((is_alpha(*ptr) || is_digit(*ptr)) && i < MAX_VAR_NAME - 1) {
        name[i++] = to_upper(*ptr);
        ptr++;
    }

    // Check for $ suffix (string variable)
    if (*ptr == '$') {
        is_string = 1;
        ptr++;
    }

    name[i] = '\0';
    return is_string;
}

// Find variable by name, return index or -1
static int find_var(const char *name, int is_string) {
    for (int i = 0; i < num_vars; i++) {
        if (variables[i].is_string == is_string && str_equal(variables[i].name, name))
            return i;
    }
    return -1;
}

// Find or create variable, return index
static int get_or_create_var(const char *name, int is_string) {
    int idx = find_var(name, is_string);
    if (idx >= 0) return idx;

    if (num_vars >= MAX_VARIABLES) return -1;

    idx = num_vars++;
    str_copy(variables[idx].name, name, MAX_VAR_NAME);
    variables[idx].is_string = is_string;
    variables[idx].is_array = 0;
    variables[idx].array_size = 0;
    if (is_string) {
        variables[idx].str_val[0] = '\0';
    } else {
        variables[idx].int_val = 0;
    }
    return idx;
}

static uint32_t rng_next(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state >> 16) & 0x7FFF;
}

static void error(const char *msg) {
    print_string(msg);
    print_newline();
    running = 0;
}

//----------------------------------------------------------------------
// Line management
//----------------------------------------------------------------------

static int find_line(int linenum) {
    for (int i = 0; i < num_lines; i++)
        if (line_nums[i] == linenum) return i;
    return -1;
}

static void insert_line(int linenum, const char *text) {
    int i;
    for (i = 0; i < num_lines; i++) {
        if (line_nums[i] == linenum) {
            str_copy(program[i], text, MAX_LINE_LEN);
            return;
        }
        if (line_nums[i] > linenum) break;
    }
    if (num_lines >= MAX_LINES) { error("OUT OF MEMORY"); return; }
    for (int j = num_lines; j > i; j--) {
        line_nums[j] = line_nums[j-1];
        str_copy(program[j], program[j-1], MAX_LINE_LEN);
    }
    line_nums[i] = linenum;
    str_copy(program[i], text, MAX_LINE_LEN);
    num_lines++;
}

static void delete_line(int linenum) {
    int idx = find_line(linenum);
    if (idx < 0) return;
    for (int i = idx; i < num_lines - 1; i++) {
        line_nums[i] = line_nums[i+1];
        str_copy(program[i], program[i+1], MAX_LINE_LEN);
    }
    num_lines--;
}

//----------------------------------------------------------------------
// Expression parser - String expressions
//----------------------------------------------------------------------

static void parse_string_literal(char *dest) {
    int i = 0;
    if (*ptr == '"') {
        ptr++;
        while (*ptr && *ptr != '"' && i < MAX_STRING_LEN - 1)
            dest[i++] = *ptr++;
        if (*ptr == '"') ptr++;
    }
    dest[i] = '\0';
}

static void str_factor(char *dest) {
    skip_spaces();
    dest[0] = '\0';

    // String literal
    if (*ptr == '"') {
        parse_string_literal(dest);
        return;
    }

    // CHR$(n)
    if (match_keyword("CHR$")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        dest[0] = (char)(n & 0xFF);
        dest[1] = '\0';
        return;
    }

    // STR$(n)
    if (match_keyword("STR$")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        // Convert int to string
        char buf[12];
        int i = 0, neg = 0;
        if (n < 0) { neg = 1; n = -n; }
        if (n == 0) buf[i++] = '0';
        else while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
        int j = 0;
        if (neg) dest[j++] = '-';
        while (i > 0) dest[j++] = buf[--i];
        dest[j] = '\0';
        return;
    }

    // LEFT$(s$, n)
    if (match_keyword("LEFT$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        skip_spaces();
        if (*ptr == ',') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        if (n < 0) n = 0;
        if (n > str_len(tmp)) n = str_len(tmp);
        for (int i = 0; i < n; i++) dest[i] = tmp[i];
        dest[n] = '\0';
        return;
    }

    // RIGHT$(s$, n)
    if (match_keyword("RIGHT$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        skip_spaces();
        if (*ptr == ',') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        int len = str_len(tmp);
        if (n < 0) n = 0;
        if (n > len) n = len;
        int start = len - n;
        for (int i = 0; i < n; i++) dest[i] = tmp[start + i];
        dest[n] = '\0';
        return;
    }

    // MID$(s$, start [, len])
    if (match_keyword("MID$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        skip_spaces();
        if (*ptr == ',') ptr++;
        int32_t start = expr();
        int32_t len = MAX_STRING_LEN;
        skip_spaces();
        if (*ptr == ',') { ptr++; len = expr(); }
        if (*ptr == ')') ptr++;
        int slen = str_len(tmp);
        if (start < 1) start = 1;
        start--;  // Convert to 0-based
        if (start >= slen) { dest[0] = '\0'; return; }
        if (len < 0) len = 0;
        if (start + len > slen) len = slen - start;
        for (int i = 0; i < len; i++) dest[i] = tmp[start + i];
        dest[len] = '\0';
        return;
    }

    // INKEY$
    if (match_keyword("INKEY$")) {
        int c = getchar_nonblock();
        if (c < 0) { dest[0] = '\0'; }
        else { dest[0] = (char)c; dest[1] = '\0'; }
        return;
    }

    // String variable (NAME$) or array (NAME$(n))
    if (is_alpha(*ptr)) {
        const char *save = ptr;
        char name[MAX_VAR_NAME];
        int is_str = parse_var_name(name);

        if (is_str) {
            skip_spaces();
            int idx = get_or_create_var(name, 1);
            if (idx < 0) { error("TOO MANY VARS"); return; }

            if (*ptr == '(') {
                // Array access
                ptr++;
                int32_t index = expr();
                if (*ptr == ')') ptr++;
                Variable *v = &variables[idx];
                if (v->is_array && index >= 0 && index < v->array_size) {
                    str_copy(dest, v->str_array[index], MAX_STRING_LEN);
                } else {
                    error("BAD SUBSCRIPT");
                }
            } else {
                str_copy(dest, variables[idx].str_val, MAX_STRING_LEN);
            }
            return;
        }
        ptr = save;  // Not a string variable, restore position
    }

    dest[0] = '\0';
}

static void str_expr(char *dest) {
    str_factor(dest);

    // String concatenation with +
    while (1) {
        skip_spaces();
        if (*ptr == '+') {
            ptr++;
            char tmp[MAX_STRING_LEN];
            str_factor(tmp);
            // Concatenate
            int len = str_len(dest);
            int i = 0;
            while (tmp[i] && len < MAX_STRING_LEN - 1)
                dest[len++] = tmp[i++];
            dest[len] = '\0';
        } else {
            break;
        }
    }
}

//----------------------------------------------------------------------
// Expression parser - Numeric expressions
//----------------------------------------------------------------------

static int32_t factor(void) {
    skip_spaces();

    if (*ptr == '(') {
        ptr++;
        int32_t result = expr();
        skip_spaces();
        if (*ptr == ')') ptr++;
        return result;
    }

    // NOT
    if (match_keyword("NOT")) {
        return !factor();
    }

    // Unary minus
    if (*ptr == '-') {
        ptr++;
        return -factor();
    }

    // Functions
    if (match_keyword("RND")) {
        skip_spaces();
        if (*ptr == '(') { ptr++; expr(); if (*ptr == ')') ptr++; }
        return rng_next() % 32768;
    }

    if (match_keyword("ABS")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        return n < 0 ? -n : n;
    }

    if (match_keyword("SGN")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        return n > 0 ? 1 : (n < 0 ? -1 : 0);
    }

    if (match_keyword("INT")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        return n;
    }

    if (match_keyword("LEN")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        return str_len(tmp);
    }

    if (match_keyword("VAL")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        // Parse number from string
        const char *p = tmp;
        int32_t result = 0, neg = 0;
        while (*p == ' ') p++;
        if (*p == '-') { neg = 1; p++; }
        while (*p >= '0' && *p <= '9') {
            result = result * 10 + (*p - '0');
            p++;
        }
        return neg ? -result : result;
    }

    if (match_keyword("ASC")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        return tmp[0] ? (unsigned char)tmp[0] : 0;
    }

    if (match_keyword("TIMER")) {
        return (int32_t)get_timer_ms();
    }

    // Variable (NAME or NAME(n))
    if (is_alpha(*ptr)) {
        char name[MAX_VAR_NAME];
        int is_str = parse_var_name(name);

        // String variable in numeric context = 0
        if (is_str) return 0;

        int idx = get_or_create_var(name, 0);
        if (idx < 0) { error("TOO MANY VARS"); return 0; }

        skip_spaces();
        if (*ptr == '(') {
            // Array access
            ptr++;
            int32_t index = expr();
            if (*ptr == ')') ptr++;
            Variable *v = &variables[idx];
            if (v->is_array && index >= 0 && index < v->array_size) {
                return v->int_array[index];
            } else {
                error("BAD SUBSCRIPT");
                return 0;
            }
        }
        return variables[idx].int_val;
    }

    // Number
    if (is_digit(*ptr)) {
        return parse_number();
    }

    return 0;
}

static int32_t term(void) {
    int32_t result = factor();
    while (1) {
        skip_spaces();
        if (*ptr == '*') { ptr++; result *= factor(); }
        else if (*ptr == '/') {
            ptr++;
            int32_t d = factor();
            if (d != 0) result /= d;
        }
        else if (match_keyword("MOD")) {
            int32_t d = factor();
            if (d != 0) result %= d;
        }
        else break;
    }
    return result;
}

static int32_t arith_expr(void) {
    int32_t result = term();
    while (1) {
        skip_spaces();
        if (*ptr == '+') { ptr++; result += term(); }
        else if (*ptr == '-') { ptr++; result -= term(); }
        else break;
    }
    return result;
}

static int32_t comp_expr(void) {
    int32_t left = arith_expr();
    skip_spaces();

    if (*ptr == '<' && *(ptr+1) == '>') { ptr += 2; return left != arith_expr(); }
    if (*ptr == '<' && *(ptr+1) == '=') { ptr += 2; return left <= arith_expr(); }
    if (*ptr == '>' && *(ptr+1) == '=') { ptr += 2; return left >= arith_expr(); }
    if (*ptr == '<') { ptr++; return left < arith_expr(); }
    if (*ptr == '>') { ptr++; return left > arith_expr(); }
    if (*ptr == '=') { ptr++; return left == arith_expr(); }

    return left;
}

static int32_t expr(void) {
    int32_t result = comp_expr();
    while (1) {
        skip_spaces();
        if (match_keyword("AND")) { result = result && comp_expr(); }
        else if (match_keyword("OR")) { result = result || comp_expr(); }
        else break;
    }
    return result;
}

//----------------------------------------------------------------------
// DATA/READ/RESTORE
//----------------------------------------------------------------------

static void find_next_data(void) {
    while (data_line < num_lines) {
        if (!data_ptr) {
            // Start scanning this line
            data_ptr = program[data_line];
        }

        // Look for DATA keyword
        while (*data_ptr) {
            while (*data_ptr == ' ' || *data_ptr == '\t') data_ptr++;

            if (to_upper(data_ptr[0]) == 'D' && to_upper(data_ptr[1]) == 'A' &&
                to_upper(data_ptr[2]) == 'T' && to_upper(data_ptr[3]) == 'A' &&
                !is_alpha(data_ptr[4])) {
                data_ptr += 4;
                while (*data_ptr == ' ' || *data_ptr == '\t') data_ptr++;
                return;  // Found DATA
            }

            // Skip to next statement
            while (*data_ptr && *data_ptr != ':') data_ptr++;
            if (*data_ptr == ':') data_ptr++;
        }

        data_line++;
        data_ptr = 0;
    }
}

static void read_data_item(int is_string, int32_t *num_val, char *str_val) {
    // Skip to next data item if at end of current DATA statement
    while (1) {
        // Need to find a DATA statement?
        if (!data_ptr || !*data_ptr || *data_ptr == ':') {
            if (data_ptr && *data_ptr == ':') data_ptr++;
            find_next_data();
        }

        if (data_line >= num_lines) {
            error("OUT OF DATA");
            return;
        }

        while (*data_ptr == ' ' || *data_ptr == '\t') data_ptr++;
        if (*data_ptr && *data_ptr != ':') break;  // Found data
    }

    if (is_string) {
        int i = 0;
        if (*data_ptr == '"') {
            data_ptr++;
            while (*data_ptr && *data_ptr != '"' && i < MAX_STRING_LEN - 1)
                str_val[i++] = *data_ptr++;
            if (*data_ptr == '"') data_ptr++;
        } else {
            while (*data_ptr && *data_ptr != ',' && *data_ptr != ':' && i < MAX_STRING_LEN - 1)
                str_val[i++] = *data_ptr++;
        }
        str_val[i] = '\0';
    } else {
        int32_t result = 0, neg = 0;
        if (*data_ptr == '-') { neg = 1; data_ptr++; }
        while (*data_ptr >= '0' && *data_ptr <= '9') {
            result = result * 10 + (*data_ptr - '0');
            data_ptr++;
        }
        *num_val = neg ? -result : result;
    }

    // Move past separator to next item
    while (*data_ptr == ' ' || *data_ptr == '\t') data_ptr++;
    if (*data_ptr == ',') data_ptr++;
}

//----------------------------------------------------------------------
// Statements
//----------------------------------------------------------------------

// Check if expression at ptr is a string expression
static int is_string_expr(void) {
    const char *p = ptr;

    // String literal
    if (*p == '"') return 1;

    // String functions: CHR$, STR$, LEFT$, RIGHT$, MID$, INKEY$
    if (to_upper(p[0]) == 'C' && to_upper(p[1]) == 'H' && to_upper(p[2]) == 'R' && p[3] == '$') return 1;
    if (to_upper(p[0]) == 'S' && to_upper(p[1]) == 'T' && to_upper(p[2]) == 'R' && p[3] == '$') return 1;
    if (to_upper(p[0]) == 'L' && to_upper(p[1]) == 'E' && to_upper(p[2]) == 'F' && to_upper(p[3]) == 'T' && p[4] == '$') return 1;
    if (to_upper(p[0]) == 'R' && to_upper(p[1]) == 'I' && to_upper(p[2]) == 'G' && to_upper(p[3]) == 'H' && to_upper(p[4]) == 'T' && p[5] == '$') return 1;
    if (to_upper(p[0]) == 'M' && to_upper(p[1]) == 'I' && to_upper(p[2]) == 'D' && p[3] == '$') return 1;
    if (to_upper(p[0]) == 'I' && to_upper(p[1]) == 'N' && to_upper(p[2]) == 'K' && to_upper(p[3]) == 'E' && to_upper(p[4]) == 'Y' && p[5] == '$') return 1;

    // String variable: NAME$ - scan alphanumeric then check for $
    if (is_alpha(*p)) {
        while (is_alpha(*p) || is_digit(*p)) p++;
        if (*p == '$') return 1;
    }

    return 0;
}

static void stmt_print(void) {
    skip_spaces();
    int need_newline = 1;

    while (*ptr && *ptr != ':') {
        skip_spaces();

        if (*ptr == ';') {
            need_newline = 0;
            ptr++;
        } else if (*ptr == ',') {
            print_char('\t');
            need_newline = 1;
            ptr++;
        } else if (*ptr == '"') {
            // String literal
            ptr++;
            while (*ptr && *ptr != '"') print_char(*ptr++);
            if (*ptr == '"') ptr++;
            need_newline = 1;
        } else if (*ptr && *ptr != ':') {
            // Check if string expression
            if (is_string_expr()) {
                char tmp[MAX_STRING_LEN];
                str_expr(tmp);
                print_string(tmp);
            } else {
                print_int(expr());
            }
            need_newline = 1;
        }
    }

    if (need_newline) print_newline();
}

static void stmt_input(void) {
    skip_spaces();

    // Optional prompt
    if (*ptr == '"') {
        ptr++;
        while (*ptr && *ptr != '"') print_char(*ptr++);
        if (*ptr == '"') ptr++;
        skip_spaces();
        if (*ptr == ';' || *ptr == ',') ptr++;
        skip_spaces();
    }

    if (!is_alpha(*ptr)) return;

    char name[MAX_VAR_NAME];
    int is_string = parse_var_name(name);

    int idx = get_or_create_var(name, is_string);
    if (idx < 0) { error("TOO MANY VARS"); return; }

    // Check for array
    int is_array_access = 0;
    int32_t index = 0;
    skip_spaces();
    if (*ptr == '(') {
        is_array_access = 1;
        ptr++;
        index = expr();
        if (*ptr == ')') ptr++;
    }

    Variable *v = &variables[idx];

    if (is_string) {
        // Read string
        char buf[MAX_STRING_LEN];
        int pos = 0;
        while (pos < MAX_STRING_LEN - 1) {
            int c = getchar();
            if (c == '\r' || c == '\n') { putchar('\n'); break; }
            if (c == 8 || c == 127) {
                if (pos > 0) { pos--; putchar(8); putchar(' '); putchar(8); }
                continue;
            }
            buf[pos++] = c;
            putchar(c);
        }
        buf[pos] = '\0';

        if (is_array_access) {
            if (v->is_array && index >= 0 && index < v->array_size)
                str_copy(v->str_array[index], buf, MAX_STRING_LEN);
        } else {
            str_copy(v->str_val, buf, MAX_STRING_LEN);
        }
    } else {
        // Read number
        int32_t result = 0, neg = 0;
        int c;
        while ((c = getchar()) == ' ' || c == '\t');
        if (c == '-') { neg = 1; c = getchar(); }
        while (c >= '0' && c <= '9') {
            putchar(c);
            result = result * 10 + (c - '0');
            c = getchar();
        }
        if (c == '\r' || c == '\n') putchar('\n');
        if (neg) result = -result;

        if (is_array_access) {
            if (v->is_array && index >= 0 && index < v->array_size)
                v->int_array[index] = result;
        } else {
            v->int_val = result;
        }
    }
}

static void stmt_let(void) {
    skip_spaces();
    if (!is_alpha(*ptr)) return;

    char name[MAX_VAR_NAME];
    int is_string = parse_var_name(name);

    int idx = get_or_create_var(name, is_string);
    if (idx < 0) { error("TOO MANY VARS"); return; }

    // Check for array
    int is_array_access = 0;
    int32_t index = 0;
    skip_spaces();
    if (*ptr == '(') {
        is_array_access = 1;
        ptr++;
        index = expr();
        if (*ptr == ')') ptr++;
    }

    skip_spaces();
    if (*ptr == '=') ptr++;

    Variable *v = &variables[idx];

    if (is_string) {
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (is_array_access) {
            if (v->is_array && index >= 0 && index < v->array_size)
                str_copy(v->str_array[index], tmp, MAX_STRING_LEN);
            else error("BAD SUBSCRIPT");
        } else {
            str_copy(v->str_val, tmp, MAX_STRING_LEN);
        }
    } else {
        int32_t val = expr();
        if (is_array_access) {
            if (v->is_array && index >= 0 && index < v->array_size)
                v->int_array[index] = val;
            else error("BAD SUBSCRIPT");
        } else {
            v->int_val = val;
        }
    }
}

// Memory allocation tracking for arrays
static uint32_t heap_str_ptr = BASIC_HEAP;          // String arrays at start
static uint32_t heap_int_ptr = BASIC_HEAP + 0x8000; // Int arrays at 32KB offset

static void stmt_dim(void) {
    while (1) {
        skip_spaces();
        if (!is_alpha(*ptr)) break;

        char name[MAX_VAR_NAME];
        int is_string = parse_var_name(name);

        skip_spaces();
        if (*ptr != '(') { error("EXPECTED ("); return; }
        ptr++;

        int32_t size = expr() + 1;  // BASIC arrays are 0 to N
        if (size < 1 || size > MAX_ARRAY_SIZE) { error("BAD SUBSCRIPT"); return; }

        if (*ptr == ')') ptr++;

        int idx = get_or_create_var(name, is_string);
        if (idx < 0) { error("TOO MANY VARS"); return; }

        Variable *v = &variables[idx];
        if (v->is_array) continue;  // Already dimensioned

        v->is_array = 1;
        v->array_size = size;

        if (is_string) {
            v->str_array = (char(*)[MAX_STRING_LEN])heap_str_ptr;
            heap_str_ptr += size * MAX_STRING_LEN;
            for (int i = 0; i < size; i++) v->str_array[i][0] = '\0';
        } else {
            v->int_array = (int32_t*)heap_int_ptr;
            heap_int_ptr += size * sizeof(int32_t);
            for (int i = 0; i < size; i++) v->int_array[i] = 0;
        }

        skip_spaces();
        if (*ptr == ',') ptr++;
        else break;
    }
}

static void stmt_goto(void) {
    int linenum = expr();
    int idx = find_line(linenum);
    if (idx >= 0) current_line = idx - 1;
    else error("LINE NOT FOUND");
}

static void stmt_gosub(void) {
    if (gosub_sp >= MAX_STACK) { error("STACK OVERFLOW"); return; }
    int linenum = expr();
    int idx = find_line(linenum);
    if (idx >= 0) {
        gosub_stack[gosub_sp++] = current_line;
        current_line = idx - 1;
    } else error("LINE NOT FOUND");
}

static void stmt_return(void) {
    if (gosub_sp <= 0) { error("RETURN WITHOUT GOSUB"); return; }
    current_line = gosub_stack[--gosub_sp];
}

static void stmt_for(void) {
    skip_spaces();
    if (!is_alpha(*ptr)) return;

    char name[MAX_VAR_NAME];
    parse_var_name(name);

    int idx = get_or_create_var(name, 0);
    if (idx < 0) { error("TOO MANY VARS"); return; }

    skip_spaces();
    if (*ptr == '=') ptr++;
    int32_t start = expr();
    variables[idx].int_val = start;

    skip_spaces();
    if (!match_keyword("TO")) { error("EXPECTED TO"); return; }
    int32_t limit = expr();

    int32_t step = 1;
    skip_spaces();
    if (match_keyword("STEP")) step = expr();

    if (for_sp >= MAX_FOR_DEPTH) { error("FOR OVERFLOW"); return; }
    for_stack[for_sp].var_idx = idx;
    for_stack[for_sp].limit = limit;
    for_stack[for_sp].step = step;
    for_stack[for_sp].return_line = current_line;
    for_sp++;
}

static void stmt_next(void) {
    skip_spaces();
    int var_idx = -1;
    if (is_alpha(*ptr)) {
        char name[MAX_VAR_NAME];
        parse_var_name(name);
        var_idx = find_var(name, 0);
    }

    if (for_sp <= 0) { error("NEXT WITHOUT FOR"); return; }
    ForFrame *f = &for_stack[for_sp - 1];
    if (var_idx >= 0 && var_idx != f->var_idx) { error("NEXT MISMATCH"); return; }

    variables[f->var_idx].int_val += f->step;
    int done = f->step > 0 ? variables[f->var_idx].int_val > f->limit
                           : variables[f->var_idx].int_val < f->limit;

    if (done) for_sp--;
    else current_line = f->return_line;
}

static void stmt_while(void) {
    if (while_sp >= MAX_STACK) { error("WHILE OVERFLOW"); return; }

    int32_t cond = expr();
    if (cond) {
        while_stack[while_sp++] = current_line;
    } else {
        // Skip to matching WEND
        int depth = 1;
        while (depth > 0 && current_line < num_lines - 1) {
            current_line++;
            const char *p = program[current_line];
            while (*p) {
                while (*p == ' ' || *p == '\t') p++;
                if (to_upper(p[0]) == 'W' && to_upper(p[1]) == 'H' &&
                    to_upper(p[2]) == 'I' && to_upper(p[3]) == 'L' &&
                    to_upper(p[4]) == 'E' && !is_alpha(p[5])) depth++;
                else if (to_upper(p[0]) == 'W' && to_upper(p[1]) == 'E' &&
                         to_upper(p[2]) == 'N' && to_upper(p[3]) == 'D' &&
                         !is_alpha(p[4])) depth--;
                while (*p && *p != ':') p++;
                if (*p == ':') p++;
            }
        }
    }
}

static void stmt_wend(void) {
    if (while_sp <= 0) { error("WEND WITHOUT WHILE"); return; }
    current_line = while_stack[--while_sp] - 1;  // Go back to WHILE line
}

static void stmt_on(void) {
    int32_t n = expr();
    skip_spaces();

    int is_gosub = 0;
    if (match_keyword("GOTO")) { is_gosub = 0; }
    else if (match_keyword("GOSUB")) { is_gosub = 1; }
    else { error("EXPECTED GOTO/GOSUB"); return; }

    // Parse line number list and find nth one
    int count = 0;
    int target = 0;
    while (1) {
        skip_spaces();
        if (!is_digit(*ptr)) break;
        count++;
        int linenum = parse_number();
        if (count == n) target = linenum;
        skip_spaces();
        if (*ptr == ',') ptr++;
        else break;
    }

    if (target == 0) return;  // n out of range, continue

    int idx = find_line(target);
    if (idx < 0) { error("LINE NOT FOUND"); return; }

    if (is_gosub) {
        if (gosub_sp >= MAX_STACK) { error("STACK OVERFLOW"); return; }
        gosub_stack[gosub_sp++] = current_line;
    }
    current_line = idx - 1;
}

static void stmt_read(void) {
    while (1) {
        skip_spaces();
        if (!is_alpha(*ptr)) break;

        char name[MAX_VAR_NAME];
        int is_string = parse_var_name(name);

        int idx = get_or_create_var(name, is_string);
        if (idx < 0) { error("TOO MANY VARS"); return; }

        // Check for array
        int is_array_access = 0;
        int32_t index = 0;
        skip_spaces();
        if (*ptr == '(') {
            is_array_access = 1;
            ptr++;
            index = expr();
            if (*ptr == ')') ptr++;
        }

        Variable *v = &variables[idx];

        if (is_string) {
            char tmp[MAX_STRING_LEN];
            int32_t dummy;
            read_data_item(1, &dummy, tmp);
            if (is_array_access && v->is_array && index >= 0 && index < v->array_size)
                str_copy(v->str_array[index], tmp, MAX_STRING_LEN);
            else if (!is_array_access)
                str_copy(v->str_val, tmp, MAX_STRING_LEN);
        } else {
            int32_t val;
            char dummy[MAX_STRING_LEN];
            read_data_item(0, &val, dummy);
            if (is_array_access && v->is_array && index >= 0 && index < v->array_size)
                v->int_array[index] = val;
            else if (!is_array_access)
                v->int_val = val;
        }

        skip_spaces();
        if (*ptr == ',') ptr++;
        else break;
    }
}

static void stmt_restore(void) {
    data_line = 0;
    data_ptr = 0;
}

//----------------------------------------------------------------------
// Graphics statements
//----------------------------------------------------------------------

// CLS - clear screen
static void stmt_cls(void) {
    display_clear();
}

// PSET x, y, color
static void stmt_pset(void) {
    skip_spaces();
    int32_t x = expr();
    skip_spaces();
    if (*ptr == ',') ptr++;
    int32_t y = expr();
    skip_spaces();
    int32_t color = 15;  // Default: white
    if (*ptr == ',') { ptr++; color = expr(); }
    display_pset(x, y, (uint8_t)color);
}

// LINE x1, y1, x2, y2, color
static void stmt_line(void) {
    skip_spaces();
    int32_t x1 = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t y1 = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t x2 = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t y2 = expr();
    skip_spaces();
    int32_t color = 15;
    if (*ptr == ',') { ptr++; color = expr(); }
    display_line(x1, y1, x2, y2, (uint8_t)color);
}

// CIRCLE x, y, r [, color]
static void stmt_circle(void) {
    skip_spaces();
    int32_t x = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t y = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t r = expr();
    skip_spaces();
    int32_t color = 15;
    if (*ptr == ',') { ptr++; color = expr(); }
    display_circle(x, y, r, (uint8_t)color);
}

// FCIRCLE x, y, r [, color] - filled circle
static void stmt_fcircle(void) {
    skip_spaces();
    int32_t x = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t y = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t r = expr();
    skip_spaces();
    int32_t color = 15;
    if (*ptr == ',') { ptr++; color = expr(); }
    display_fill_circle(x, y, r, (uint8_t)color);
}

// PAINT x, y, fill_color, border_color
static void stmt_paint(void) {
    skip_spaces();
    int32_t x = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t y = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t fill = expr();
    skip_spaces(); if (*ptr == ',') ptr++;
    int32_t border = expr();
    display_paint(x, y, (uint8_t)fill, (uint8_t)border);
}

static void stmt_if(void) {
    int32_t cond = expr();
    skip_spaces();

    if (!match_keyword("THEN")) { error("EXPECTED THEN"); return; }

    if (cond) {
        skip_spaces();
        if (is_digit(*ptr)) {
            int linenum = parse_number();
            int idx = find_line(linenum);
            if (idx >= 0) current_line = idx - 1;
        } else {
            execute_line(ptr);
        }
    } else {
        // Skip to ELSE if present, or end of statement
        while (*ptr) {
            if (to_upper(ptr[0]) == 'E' && to_upper(ptr[1]) == 'L' &&
                to_upper(ptr[2]) == 'S' && to_upper(ptr[3]) == 'E' &&
                !is_alpha(ptr[4])) {
                ptr += 4;
                skip_spaces();
                execute_line(ptr);
                break;
            }
            ptr++;
        }
    }
    while (*ptr && *ptr != ':') ptr++;
}

static void execute_line(const char *line) {
    ptr = line;

    while (*ptr) {
        skip_spaces();
        if (!*ptr || *ptr == ':') { if (*ptr == ':') ptr++; continue; }

        if (match_keyword("REM") || *ptr == '\'') return;
        if (match_keyword("DATA")) { while (*ptr && *ptr != ':') ptr++; continue; }

        if (match_keyword("PRINT") || *ptr == '?') {
            if (*ptr == '?') ptr++;
            stmt_print();
        }
        else if (match_keyword("LET")) stmt_let();
        else if (match_keyword("INPUT")) stmt_input();
        else if (match_keyword("DIM")) stmt_dim();
        else if (match_keyword("GOTO")) { stmt_goto(); return; }
        else if (match_keyword("GOSUB")) { stmt_gosub(); return; }
        else if (match_keyword("RETURN")) { stmt_return(); return; }
        else if (match_keyword("FOR")) stmt_for();
        else if (match_keyword("NEXT")) stmt_next();
        else if (match_keyword("WHILE")) { stmt_while(); return; }
        else if (match_keyword("WEND")) { stmt_wend(); return; }
        else if (match_keyword("IF")) { stmt_if(); return; }
        else if (match_keyword("ON")) { stmt_on(); return; }
        else if (match_keyword("READ")) stmt_read();
        else if (match_keyword("RESTORE")) stmt_restore();
        else if (match_keyword("CLS")) stmt_cls();
        else if (match_keyword("PSET")) stmt_pset();
        else if (match_keyword("LINE")) stmt_line();
        else if (match_keyword("CIRCLE")) stmt_circle();
        else if (match_keyword("FCIRCLE")) stmt_fcircle();
        else if (match_keyword("PAINT")) stmt_paint();
        else if (match_keyword("END") || match_keyword("STOP")) { running = 0; return; }
        else if (is_alpha(*ptr)) stmt_let();  // Implicit LET
        else { while (*ptr && *ptr != ':') ptr++; }

        skip_spaces();
        if (*ptr == ':') ptr++;
    }
}

//----------------------------------------------------------------------
// Commands
//----------------------------------------------------------------------

static void cmd_list(void) {
    for (int i = 0; i < num_lines; i++) {
        print_int(line_nums[i]);
        print_char(' ');
        print_string(program[i]);
        print_newline();
    }
}

static void cmd_run(void) {
    // Reset all variables to default values
    for (int i = 0; i < num_vars; i++) {
        if (variables[i].is_string) {
            if (variables[i].is_array) {
                for (int j = 0; j < variables[i].array_size; j++)
                    variables[i].str_array[j][0] = '\0';
            } else {
                variables[i].str_val[0] = '\0';
            }
        } else {
            if (variables[i].is_array) {
                for (int j = 0; j < variables[i].array_size; j++)
                    variables[i].int_array[j] = 0;
            } else {
                variables[i].int_val = 0;
            }
        }
    }
    gosub_sp = for_sp = while_sp = 0;
    data_line = 0; data_ptr = 0;
    current_line = 0;
    running = 1;

    while (running && current_line < num_lines) {
        execute_line(program[current_line]);
        current_line++;
    }
    running = 0;
}

static void cmd_new(void) {
    num_lines = 0;
    num_vars = 0;
    heap_str_ptr = BASIC_HEAP;
    heap_int_ptr = BASIC_HEAP + 0x8000;
}

static void cmd_load(const char *filename) {
    print_string("Loading: "); print_string(filename); print_newline();
    int32_t size = tftp_get(filename);
    if (size <= 0) { error("LOAD ERROR"); return; }

    cmd_new();
    const char *p = (const char *)FILE_BUF;
    const char *end = p + size;

    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
        if (p >= end) break;

        int linenum = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            linenum = linenum * 10 + (*p - '0');
            p++;
        }
        if (linenum == 0) { while (p < end && *p != '\n') p++; continue; }

        while (p < end && (*p == ' ' || *p == '\t')) p++;

        char linebuf[MAX_LINE_LEN];
        int len = 0;
        while (p < end && *p != '\n' && *p != '\r' && len < MAX_LINE_LEN - 1)
            linebuf[len++] = *p++;
        linebuf[len] = '\0';
        while (p < end && (*p == '\r' || *p == '\n')) p++;

        insert_line(linenum, linebuf);
    }
    print_string("OK\n");
}

static void cmd_save(const char *filename) {
    char *buf = (char *)FILE_BUF;
    int pos = 0;

    for (int i = 0; i < num_lines; i++) {
        // Write line number
        int ln = line_nums[i];
        char numbuf[8];
        int ni = 0;
        if (ln == 0) numbuf[ni++] = '0';
        else while (ln > 0) { numbuf[ni++] = '0' + (ln % 10); ln /= 10; }
        while (ni > 0) buf[pos++] = numbuf[--ni];
        buf[pos++] = ' ';

        // Write line content
        const char *p = program[i];
        while (*p) buf[pos++] = *p++;
        buf[pos++] = '\n';
    }

    int32_t result = tftp_put(filename, buf, pos);
    if (result < 0) error("SAVE ERROR");
    else print_string("OK\n");
}

//----------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------

void basic_main(const char *filename) {
    print_string("COSMO-32 BASIC v2.0\n");
    cmd_new();
    rng_state = 12345;

    if (filename && *filename) {
        cmd_load(filename);
        cmd_run();
        return;
    }

    char line[MAX_LINE_LEN];
    while (1) {
        print_string("] ");

        int pos = 0;
        while (pos < MAX_LINE_LEN - 1) {
            int c = getchar();
            if (c == '\r' || c == '\n') { putchar('\n'); break; }
            if (c == 8 || c == 127) {
                if (pos > 0) { pos--; putchar(8); putchar(' '); putchar(8); }
                continue;
            }
            line[pos++] = c;
            putchar(c);
        }
        line[pos] = '\0';
        if (pos == 0) continue;

        ptr = line;
        skip_spaces();

        if (is_digit(*ptr)) {
            int linenum = parse_number();
            skip_spaces();
            if (*ptr == '\0') delete_line(linenum);
            else insert_line(linenum, ptr);
            continue;
        }

        if (match_keyword("RUN")) cmd_run();
        else if (match_keyword("LIST")) cmd_list();
        else if (match_keyword("NEW")) { cmd_new(); print_string("OK\n"); }
        else if (match_keyword("LOAD")) {
            skip_spaces();
            if (*ptr == '"') ptr++;
            char fname[64];
            int i = 0;
            while (*ptr && *ptr != '"' && i < 63) fname[i++] = *ptr++;
            fname[i] = '\0';
            cmd_load(fname);
        }
        else if (match_keyword("SAVE")) {
            skip_spaces();
            if (*ptr == '"') ptr++;
            char fname[64];
            int i = 0;
            while (*ptr && *ptr != '"' && i < 63) fname[i++] = *ptr++;
            fname[i] = '\0';
            cmd_save(fname);
        }
        else if (match_keyword("BYE") || match_keyword("EXIT") || match_keyword("QUIT")) break;
        else {
            ptr = line;
            running = 1;
            execute_line(line);
            running = 0;
        }
    }
}
