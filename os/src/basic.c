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
extern void display_set_cursor(int x, int y);
extern void display_set_color(uint8_t fg, uint8_t bg);

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
static int jump_pending = 0;  // Set by NEXT/GOTO etc to prevent current_line++
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

// DO stack
typedef struct {
    int return_line;       // Line of DO statement
    int cond_at_start;     // 1 if WHILE/UNTIL after DO, 0 if at LOOP
    int is_until;          // 1 if UNTIL, 0 if WHILE
} DoFrame;
static DoFrame do_stack[MAX_STACK];
static int do_sp = 0;

// Block-IF stack
typedef struct {
    int branch_taken;      // 1 if a branch (IF/ELSEIF) was already taken
} IfFrame;
static IfFrame if_stack[MAX_STACK];
static int if_sp = 0;

// SELECT CASE stack
typedef struct {
    int is_string;
    int32_t int_val;
    char str_val[MAX_STRING_LEN];
    int case_matched;      // 1 if a CASE has already matched
} SelectFrame;
static SelectFrame select_stack[MAX_STACK];
static int select_sp = 0;

// SUB/FUNCTION definitions
#define MAX_SUBS        32
#define MAX_SUB_PARAMS  8

typedef struct {
    char name[MAX_VAR_NAME];
    int start_line;        // Line index of SUB/FUNCTION statement
    int is_function;       // 1 if FUNCTION, 0 if SUB
    int num_params;
    char params[MAX_SUB_PARAMS][MAX_VAR_NAME];
    int param_is_string[MAX_SUB_PARAMS];  // 1 if param ends with $
} SubDef;
static SubDef subs[MAX_SUBS];
static int num_subs = 0;

// SUB/FUNCTION call stack
typedef struct {
    int return_line;       // Line to return to after END SUB/FUNCTION
    int sub_idx;           // Index into subs[]
    int32_t saved_int_vals[MAX_SUB_PARAMS];      // Saved values of param vars
    char saved_str_vals[MAX_SUB_PARAMS][MAX_STRING_LEN];
    int32_t func_return_val;       // Return value for FUNCTION (integer)
    char func_return_str[MAX_STRING_LEN];  // Return value for FUNCTION (string)
} CallFrame;
static CallFrame call_stack[MAX_STACK];
static int call_sp = 0;

// DATA/READ state
static int data_line = 0;
static const char *data_ptr = 0;

// RNG state
static uint32_t rng_state = 12345;

// Forward declarations
static int32_t expr(void);
static int is_string_expr(void);
static void str_expr(char *dest);
static void execute_line(const char *line);
static int find_sub(const char *name);
static int32_t call_sub_or_func(int sub_idx, int return_str, char *str_result);

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
// SUB/FUNCTION management
//----------------------------------------------------------------------

// Find SUB/FUNCTION by name
static int find_sub(const char *name) {
    for (int i = 0; i < num_subs; i++) {
        if (str_equal(subs[i].name, name)) return i;
    }
    return -1;
}

// Scan program for SUB/FUNCTION definitions
static void scan_subs(void) {
    num_subs = 0;

    for (int line = 0; line < num_lines; line++) {
        const char *p = program[line];
        while (*p == ' ' || *p == '\t') p++;

        int is_func = 0;
        if (to_upper(p[0]) == 'S' && to_upper(p[1]) == 'U' &&
            to_upper(p[2]) == 'B' && !is_alpha(p[3])) {
            p += 3;
        } else if (to_upper(p[0]) == 'F' && to_upper(p[1]) == 'U' &&
                   to_upper(p[2]) == 'N' && to_upper(p[3]) == 'C' &&
                   to_upper(p[4]) == 'T' && to_upper(p[5]) == 'I' &&
                   to_upper(p[6]) == 'O' && to_upper(p[7]) == 'N' && !is_alpha(p[8])) {
            p += 8;
            is_func = 1;
        } else {
            continue;
        }

        if (num_subs >= MAX_SUBS) continue;

        while (*p == ' ' || *p == '\t') p++;

        // Parse name
        SubDef *s = &subs[num_subs];
        int ni = 0;
        while ((is_alpha(*p) || is_digit(*p)) && ni < MAX_VAR_NAME - 1) {
            s->name[ni++] = to_upper(*p++);
        }
        s->name[ni] = '\0';
        s->start_line = line;
        s->is_function = is_func;
        s->num_params = 0;

        // Skip optional $ for function name
        if (*p == '$') p++;

        while (*p == ' ' || *p == '\t') p++;

        // Parse parameters
        if (*p == '(') {
            p++;
            while (*p && *p != ')' && s->num_params < MAX_SUB_PARAMS) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == ')') break;

                // Parse param name
                int pi = 0;
                while ((is_alpha(*p) || is_digit(*p)) && pi < MAX_VAR_NAME - 1) {
                    s->params[s->num_params][pi++] = to_upper(*p++);
                }
                s->params[s->num_params][pi] = '\0';
                s->param_is_string[s->num_params] = 0;
                if (*p == '$') { s->param_is_string[s->num_params] = 1; p++; }
                s->num_params++;

                while (*p == ' ' || *p == '\t') p++;
                if (*p == ',') p++;
            }
        }

        num_subs++;
    }
}

// Skip to matching END SUB or END FUNCTION
static void skip_to_end_sub(int is_func) {
    int depth = 1;
    while (depth > 0 && current_line < num_lines - 1) {
        current_line++;
        const char *p = program[current_line];
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;

            // Check for nested SUB/FUNCTION
            if ((to_upper(p[0]) == 'S' && to_upper(p[1]) == 'U' &&
                 to_upper(p[2]) == 'B' && !is_alpha(p[3])) ||
                (to_upper(p[0]) == 'F' && to_upper(p[1]) == 'U' &&
                 to_upper(p[2]) == 'N' && to_upper(p[3]) == 'C' &&
                 to_upper(p[4]) == 'T' && to_upper(p[5]) == 'I' &&
                 to_upper(p[6]) == 'O' && to_upper(p[7]) == 'N' && !is_alpha(p[8]))) {
                depth++;
            }
            // Check for END SUB / END FUNCTION
            else if (to_upper(p[0]) == 'E' && to_upper(p[1]) == 'N' &&
                     to_upper(p[2]) == 'D' && (p[3] == ' ' || p[3] == '\t')) {
                const char *q = p + 4;
                while (*q == ' ' || *q == '\t') q++;
                if (to_upper(q[0]) == 'S' && to_upper(q[1]) == 'U' &&
                    to_upper(q[2]) == 'B' && !is_alpha(q[3])) {
                    depth--;
                } else if (to_upper(q[0]) == 'F' && to_upper(q[1]) == 'U' &&
                           to_upper(q[2]) == 'N' && to_upper(q[3]) == 'C' &&
                           to_upper(q[4]) == 'T' && to_upper(q[5]) == 'I' &&
                           to_upper(q[6]) == 'O' && to_upper(q[7]) == 'N' && !is_alpha(q[8])) {
                    depth--;
                }
                if (depth == 0) return;
            }

            while (*p && *p != ':') p++;
            if (*p == ':') p++;
        }
    }
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

    // UCASE$(s$)
    if (match_keyword("UCASE$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        for (int i = 0; tmp[i]; i++)
            dest[i] = (tmp[i] >= 'a' && tmp[i] <= 'z') ? tmp[i] - 32 : tmp[i];
        dest[str_len(tmp)] = '\0';
        return;
    }

    // LCASE$(s$)
    if (match_keyword("LCASE$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        for (int i = 0; tmp[i]; i++)
            dest[i] = (tmp[i] >= 'A' && tmp[i] <= 'Z') ? tmp[i] + 32 : tmp[i];
        dest[str_len(tmp)] = '\0';
        return;
    }

    // LTRIM$(s$)
    if (match_keyword("LTRIM$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        int i = 0;
        while (tmp[i] == ' ' || tmp[i] == '\t') i++;
        str_copy(dest, tmp + i, MAX_STRING_LEN);
        return;
    }

    // RTRIM$(s$)
    if (match_keyword("RTRIM$")) {
        if (*ptr == '(') ptr++;
        char tmp[MAX_STRING_LEN];
        str_expr(tmp);
        if (*ptr == ')') ptr++;
        str_copy(dest, tmp, MAX_STRING_LEN);
        int len = str_len(dest);
        while (len > 0 && (dest[len-1] == ' ' || dest[len-1] == '\t')) len--;
        dest[len] = '\0';
        return;
    }

    // SPACE$(n)
    if (match_keyword("SPACE$")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        if (n < 0) n = 0;
        if (n > MAX_STRING_LEN - 1) n = MAX_STRING_LEN - 1;
        for (int i = 0; i < n; i++) dest[i] = ' ';
        dest[n] = '\0';
        return;
    }

    // STRING$(n, char) or STRING$(n, char$)
    if (match_keyword("STRING$")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        skip_spaces();
        if (*ptr == ',') ptr++;
        skip_spaces();
        char c;
        if (*ptr == '"') {
            char tmp[MAX_STRING_LEN];
            str_expr(tmp);
            c = tmp[0] ? tmp[0] : ' ';
        } else {
            c = (char)expr();
        }
        if (*ptr == ')') ptr++;
        if (n < 0) n = 0;
        if (n > MAX_STRING_LEN - 1) n = MAX_STRING_LEN - 1;
        for (int i = 0; i < n; i++) dest[i] = c;
        dest[n] = '\0';
        return;
    }

    // HEX$(n)
    if (match_keyword("HEX$")) {
        if (*ptr == '(') ptr++;
        uint32_t n = (uint32_t)expr();
        if (*ptr == ')') ptr++;
        if (n == 0) { dest[0] = '0'; dest[1] = '\0'; return; }
        char buf[12];
        int i = 0;
        while (n > 0) {
            int d = n & 0xF;
            buf[i++] = d < 10 ? '0' + d : 'A' + d - 10;
            n >>= 4;
        }
        int j = 0;
        while (i > 0) dest[j++] = buf[--i];
        dest[j] = '\0';
        return;
    }

    // OCT$(n)
    if (match_keyword("OCT$")) {
        if (*ptr == '(') ptr++;
        uint32_t n = (uint32_t)expr();
        if (*ptr == ')') ptr++;
        if (n == 0) { dest[0] = '0'; dest[1] = '\0'; return; }
        char buf[16];
        int i = 0;
        while (n > 0) { buf[i++] = '0' + (n & 7); n >>= 3; }
        int j = 0;
        while (i > 0) dest[j++] = buf[--i];
        dest[j] = '\0';
        return;
    }

    // INPUT$(n)
    if (match_keyword("INPUT$")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        if (n < 0) n = 0;
        if (n > MAX_STRING_LEN - 1) n = MAX_STRING_LEN - 1;
        for (int i = 0; i < n; i++) {
            int c = getchar();
            dest[i] = (char)c;
        }
        dest[n] = '\0';
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

    if (match_keyword("FIX")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        return n;  // For integers, FIX = identity (truncate toward 0)
    }

    if (match_keyword("SQR")) {
        if (*ptr == '(') ptr++;
        int32_t n = expr();
        if (*ptr == ')') ptr++;
        if (n < 0) return 0;
        // Integer square root via Newton's method
        if (n == 0) return 0;
        int32_t x = n, y = (x + 1) / 2;
        while (y < x) { x = y; y = (x + n / x) / 2; }
        return x;
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

    // INSTR([start,] string$, search$)
    if (match_keyword("INSTR")) {
        if (*ptr == '(') ptr++;
        skip_spaces();
        int32_t start = 1;
        char haystack[MAX_STRING_LEN];
        char needle[MAX_STRING_LEN];

        // Check if first arg is numeric (start position)
        if (is_digit(*ptr) || (*ptr == '-' && is_digit(ptr[1]))) {
            start = expr();
            skip_spaces();
            if (*ptr == ',') ptr++;
            skip_spaces();
        } else if (is_alpha(*ptr)) {
            // Could be variable or string - check if followed by $ or (
            const char *q = ptr;
            while (is_alpha(*q) || is_digit(*q)) q++;
            if (*q != '$' && *q != '(') {
                // It's a numeric variable (start position)
                start = expr();
                skip_spaces();
                if (*ptr == ',') ptr++;
                skip_spaces();
            }
        }

        str_expr(haystack);
        skip_spaces();
        if (*ptr == ',') ptr++;
        str_expr(needle);
        if (*ptr == ')') ptr++;

        if (start < 1) start = 1;
        int hlen = str_len(haystack);
        int nlen = str_len(needle);
        if (nlen == 0) return start;
        if (start > hlen) return 0;

        for (int i = start - 1; i <= hlen - nlen; i++) {
            int match = 1;
            for (int j = 0; j < nlen; j++) {
                if (haystack[i + j] != needle[j]) { match = 0; break; }
            }
            if (match) return i + 1;  // 1-based
        }
        return 0;
    }

    if (match_keyword("TIMER")) {
        return (int32_t)get_timer_ms();
    }

    // Variable (NAME or NAME(n)) or FUNCTION call
    if (is_alpha(*ptr)) {
        char name[MAX_VAR_NAME];
        int is_str = parse_var_name(name);

        // String variable in numeric context = 0
        if (is_str) return 0;

        skip_spaces();

        // Check if it's a FUNCTION call
        if (*ptr == '(') {
            int sub_idx = find_sub(name);
            if (sub_idx >= 0 && subs[sub_idx].is_function) {
                // It's a FUNCTION call
                return call_sub_or_func(sub_idx, 0, 0);
            }

            // Array access
            int idx = get_or_create_var(name, 0);
            if (idx < 0) { error("TOO MANY VARS"); return 0; }
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

        int idx = get_or_create_var(name, 0);
        if (idx < 0) { error("TOO MANY VARS"); return 0; }
        return variables[idx].int_val;
    }

    // Number
    if (is_digit(*ptr)) {
        return parse_number();
    }

    return 0;
}

static int32_t power_expr(void) {
    int32_t base = factor();
    skip_spaces();
    if (*ptr == '^') {
        ptr++;
        int32_t exp = power_expr();  // Right-associative: 2^3^2 = 2^(3^2)
        int32_t result = 1;
        int neg = (exp < 0);
        if (neg) exp = -exp;
        while (exp-- > 0) result *= base;
        return neg ? (base != 0 ? 1 / result : 0) : result;
    }
    return base;
}

static int32_t term(void) {
    int32_t result = power_expr();
    while (1) {
        skip_spaces();
        if (*ptr == '*') { ptr++; result *= power_expr(); }
        else if (*ptr == '/') {
            ptr++;
            int32_t d = power_expr();
            if (d != 0) result /= d;
        }
        else if (*ptr == '\\') {
            ptr++;
            int32_t d = power_expr();
            if (d != 0) result /= d;  // Integer division (same as / for integers)
        }
        else if (match_keyword("MOD")) {
            int32_t d = power_expr();
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
    skip_spaces();
    // Check if this is a string comparison
    if (is_string_expr()) {
        char left[MAX_STRING_LEN], right[MAX_STRING_LEN];
        str_expr(left);
        skip_spaces();
        int op = 0;  // 0=none, 1==, 2=<>, 3=<, 4=>, 5=<=, 6=>=
        if (*ptr == '<' && *(ptr+1) == '>') { ptr += 2; op = 2; }
        else if (*ptr == '<' && *(ptr+1) == '=') { ptr += 2; op = 5; }
        else if (*ptr == '>' && *(ptr+1) == '=') { ptr += 2; op = 6; }
        else if (*ptr == '<') { ptr++; op = 3; }
        else if (*ptr == '>') { ptr++; op = 4; }
        else if (*ptr == '=') { ptr++; op = 1; }
        if (op == 0) return left[0] ? -1 : 0;  // Non-empty string = true
        str_expr(right);
        int cmp = 0;
        for (int i = 0; ; i++) {
            if (left[i] != right[i]) { cmp = (unsigned char)left[i] - (unsigned char)right[i]; break; }
            if (left[i] == '\0') break;
        }
        switch (op) {
            case 1: return cmp == 0;
            case 2: return cmp != 0;
            case 3: return cmp < 0;
            case 4: return cmp > 0;
            case 5: return cmp <= 0;
            case 6: return cmp >= 0;
        }
        return 0;
    }
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
        if (match_keyword("AND")) {
            int32_t right = comp_expr();
            result = result && right;
        }
        else if (match_keyword("OR")) {
            int32_t right = comp_expr();
            result = result || right;
        }
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

    // String functions or variables ending with $
    if (is_alpha(*p)) {
        while (is_alpha(*p) || is_digit(*p)) p++;
        if (*p == '$') return 1;
    }

    return 0;
}

static int print_col = 0;  // Track column position for TAB

static void print_char_track(char c) {
    putchar(c);
    if (c == '\n') print_col = 0;
    else if (c == '\t') print_col = (print_col + 8) & ~7;
    else print_col++;
}

// PRINT USING helper: format integer with width
static void print_using_int(const char *fmt, int fmtlen, int32_t val) {
    // Count # characters for width
    int width = 0;
    for (int i = 0; i < fmtlen; i++) {
        if (fmt[i] == '#') width++;
    }
    if (width == 0) width = 1;

    // Convert number to string
    char buf[16];
    int len = 0, neg = 0;
    if (val < 0) { neg = 1; val = -val; }
    if (val == 0) buf[len++] = '0';
    else while (val > 0) { buf[len++] = '0' + (val % 10); val /= 10; }

    // Print with padding
    int total = len + (neg ? 1 : 0);
    for (int i = 0; i < width - total; i++) print_char_track(' ');
    if (neg) print_char_track('-');
    while (len > 0) print_char_track(buf[--len]);
}

// PRINT USING helper: format string with width
static void print_using_str(int width, const char *val) {
    int vlen = str_len(val);
    for (int i = 0; i < width && i < vlen; i++) print_char_track(val[i]);
    for (int i = vlen; i < width; i++) print_char_track(' ');
}

static void stmt_print(void) {
    skip_spaces();

    // Check for PRINT USING
    if (match_keyword("USING")) {
        skip_spaces();
        char fmt[MAX_STRING_LEN];
        str_expr(fmt);
        skip_spaces();
        if (*ptr == ';') ptr++;

        // Process format string and values
        const char *f = fmt;
        while (*f && *ptr && *ptr != ':') {
            skip_spaces();
            if (*f == '#' || *f == '+' || *f == '-' || *f == '$') {
                // Numeric format - collect format specifier
                const char *start = f;
                while (*f == '#' || *f == '.' || *f == ',' || *f == '+' || *f == '-' || *f == '$') f++;
                int fmtlen = f - start;
                int32_t val = expr();
                print_using_int(start, fmtlen, val);
                skip_spaces();
                if (*ptr == ',' || *ptr == ';') ptr++;
            } else if (*f == '\\') {
                // String format: \   \ (width = spaces + 2)
                f++;  // Skip first backslash
                int width = 2;
                while (*f && *f != '\\') { f++; width++; }
                if (*f == '\\') f++;
                char val[MAX_STRING_LEN];
                str_expr(val);
                print_using_str(width, val);
                skip_spaces();
                if (*ptr == ',' || *ptr == ';') ptr++;
            } else {
                // Literal character in format
                print_char_track(*f++);
            }
        }
        print_char_track('\n');
        return;
    }

    int need_newline = 1;

    while (*ptr && *ptr != ':') {
        skip_spaces();

        if (*ptr == ';') {
            need_newline = 0;
            ptr++;
        } else if (*ptr == ',') {
            print_char_track('\t');
            need_newline = 1;
            ptr++;
        } else if (match_keyword("TAB")) {
            // TAB(n) - move to column n
            if (*ptr == '(') ptr++;
            int32_t col = expr();
            if (*ptr == ')') ptr++;
            if (col < 1) col = 1;
            if (col > 80) col = 80;
            while (print_col < col - 1) print_char_track(' ');
            need_newline = 0;
        } else if (match_keyword("SPC")) {
            // SPC(n) - print n spaces
            if (*ptr == '(') ptr++;
            int32_t n = expr();
            if (*ptr == ')') ptr++;
            if (n < 0) n = 0;
            for (int i = 0; i < n; i++) print_char_track(' ');
            need_newline = 0;
        } else if (*ptr == '"') {
            // String literal
            ptr++;
            while (*ptr && *ptr != '"') print_char_track(*ptr++);
            if (*ptr == '"') ptr++;
            need_newline = 1;
        } else if (*ptr && *ptr != ':') {
            // Check if string expression
            if (is_string_expr()) {
                char tmp[MAX_STRING_LEN];
                str_expr(tmp);
                for (int i = 0; tmp[i]; i++) print_char_track(tmp[i]);
            } else {
                // Print integer
                int32_t n = expr();
                char buf[12];
                int i = 0, neg = 0;
                if (n < 0) { neg = 1; n = -n; }
                if (n == 0) buf[i++] = '0';
                else while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
                if (neg) print_char_track('-');
                while (i > 0) print_char_track(buf[--i]);
            }
            need_newline = 1;
        }
    }

    if (need_newline) { print_char_track('\n'); }
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

    // Check for array access or FUNCTION call (but we need the = sign)
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

    // Check if this is a FUNCTION return value assignment
    if (call_sp > 0 && !is_array_access) {
        CallFrame *f = &call_stack[call_sp - 1];
        SubDef *s = &subs[f->sub_idx];
        if (s->is_function && str_equal(name, s->name)) {
            // Assigning to FUNCTION name = setting return value
            if (is_string) {
                str_expr(f->func_return_str);
            } else {
                f->func_return_val = expr();
            }
            return;
        }
    }

    int idx = get_or_create_var(name, is_string);
    if (idx < 0) { error("TOO MANY VARS"); return; }

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

    // Check if we're re-entering an existing FOR loop on same line (inline loop)
    if (for_sp > 0 && for_stack[for_sp-1].var_idx == idx
                   && for_stack[for_sp-1].return_line == current_line) {
        // Skip past the FOR initialization - we're in a loop iteration
        skip_spaces(); if (*ptr == '=') ptr++;
        expr();  // skip start value
        skip_spaces(); match_keyword("TO"); expr();  // skip limit
        skip_spaces(); if (match_keyword("STEP")) expr();  // skip step
        return;
    }

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
    else { current_line = f->return_line; jump_pending = 1; }
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

// Skip to matching LOOP
static void skip_to_loop(void) {
    int depth = 1;
    while (depth > 0 && current_line < num_lines - 1) {
        current_line++;
        const char *p = program[current_line];
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (to_upper(p[0]) == 'D' && to_upper(p[1]) == 'O' && !is_alpha(p[2])) {
                depth++;
            } else if (to_upper(p[0]) == 'L' && to_upper(p[1]) == 'O' &&
                       to_upper(p[2]) == 'O' && to_upper(p[3]) == 'P' && !is_alpha(p[4])) {
                depth--;
                if (depth == 0) return;
            }
            while (*p && *p != ':') p++;
            if (*p == ':') p++;
        }
    }
}

static void stmt_do(void) {
    if (do_sp >= MAX_STACK) { error("DO OVERFLOW"); return; }

    skip_spaces();
    int cond_at_start = 0;
    int is_until = 0;
    int32_t cond = 1;

    if (match_keyword("WHILE")) {
        cond_at_start = 1;
        is_until = 0;
        cond = expr();
    } else if (match_keyword("UNTIL")) {
        cond_at_start = 1;
        is_until = 1;
        cond = !expr();
    }

    do_stack[do_sp].return_line = current_line;
    do_stack[do_sp].cond_at_start = cond_at_start;
    do_stack[do_sp].is_until = is_until;
    do_sp++;

    if (cond_at_start && !cond) {
        do_sp--;
        skip_to_loop();
    }
}

static void stmt_loop(void) {
    if (do_sp <= 0) { error("LOOP WITHOUT DO"); return; }

    DoFrame *f = &do_stack[do_sp - 1];
    int return_line = f->return_line;
    skip_spaces();

    if (f->cond_at_start) {
        // Condition was at DO, pop and loop back (DO will push again if cond true)
        do_sp--;
        current_line = return_line - 1;
    } else {
        // Check condition at LOOP
        int32_t cond = 1;
        if (match_keyword("WHILE")) {
            cond = expr();
        } else if (match_keyword("UNTIL")) {
            cond = !expr();
        }

        // Always pop - DO will push again if we loop back
        do_sp--;
        if (cond) {
            current_line = return_line - 1;
        }
    }
}

static void stmt_exit_do(void) {
    if (do_sp <= 0) { error("EXIT DO WITHOUT DO"); return; }
    do_sp--;
    skip_to_loop();
}

// Skip to matching NEXT
static void skip_to_next(void) {
    int depth = 1;
    while (depth > 0 && current_line < num_lines - 1) {
        current_line++;
        const char *p = program[current_line];
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (to_upper(p[0]) == 'F' && to_upper(p[1]) == 'O' &&
                to_upper(p[2]) == 'R' && !is_alpha(p[3])) {
                depth++;
            } else if (to_upper(p[0]) == 'N' && to_upper(p[1]) == 'E' &&
                       to_upper(p[2]) == 'X' && to_upper(p[3]) == 'T' && !is_alpha(p[4])) {
                depth--;
                if (depth == 0) return;
            }
            while (*p && *p != ':') p++;
            if (*p == ':') p++;
        }
    }
}

static void stmt_exit_for(void) {
    if (for_sp <= 0) { error("EXIT FOR WITHOUT FOR"); return; }
    for_sp--;
    skip_to_next();
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
// SUB/FUNCTION statements
//----------------------------------------------------------------------

// DECLARE SUB/FUNCTION - just skip (definitions scanned at RUN)
static void stmt_declare(void) {
    while (*ptr && *ptr != ':') ptr++;
}

// SUB name(params) - skip to END SUB (body executed via CALL)
static void stmt_sub_def(void) {
    skip_to_end_sub(0);
}

// FUNCTION name(params) - skip to END FUNCTION
static void stmt_func_def(void) {
    skip_to_end_sub(1);
}

// END SUB - return from subroutine
static void stmt_end_sub(void) {
    if (call_sp <= 0) { error("END SUB WITHOUT CALL"); return; }
    call_sp--;
    CallFrame *f = &call_stack[call_sp];

    // Restore parameter variables
    SubDef *s = &subs[f->sub_idx];
    for (int i = 0; i < s->num_params; i++) {
        int idx = find_var(s->params[i], s->param_is_string[i]);
        if (idx >= 0) {
            if (s->param_is_string[i]) {
                str_copy(variables[idx].str_val, f->saved_str_vals[i], MAX_STRING_LEN);
            } else {
                variables[idx].int_val = f->saved_int_vals[i];
            }
        }
    }

    current_line = f->return_line;
}

// END FUNCTION - return from function
static void stmt_end_func(void) {
    if (call_sp <= 0) { error("END FUNCTION WITHOUT CALL"); return; }
    call_sp--;
    CallFrame *f = &call_stack[call_sp];

    // Restore parameter variables
    SubDef *s = &subs[f->sub_idx];
    for (int i = 0; i < s->num_params; i++) {
        int idx = find_var(s->params[i], s->param_is_string[i]);
        if (idx >= 0) {
            if (s->param_is_string[i]) {
                str_copy(variables[idx].str_val, f->saved_str_vals[i], MAX_STRING_LEN);
            } else {
                variables[idx].int_val = f->saved_int_vals[i];
            }
        }
    }

    current_line = f->return_line;
}

// Helper: call a SUB or FUNCTION
static int32_t call_sub_or_func(int sub_idx, int return_str, char *str_result) {
    SubDef *s = &subs[sub_idx];

    if (call_sp >= MAX_STACK) { error("CALL OVERFLOW"); return 0; }

    CallFrame *f = &call_stack[call_sp];
    f->return_line = current_line;
    f->sub_idx = sub_idx;
    f->func_return_val = 0;
    f->func_return_str[0] = '\0';

    // Save current values of parameter variables and set new values
    skip_spaces();
    if (*ptr == '(') ptr++;

    for (int i = 0; i < s->num_params; i++) {
        int idx = get_or_create_var(s->params[i], s->param_is_string[i]);
        if (idx >= 0) {
            // Save old value
            if (s->param_is_string[i]) {
                str_copy(f->saved_str_vals[i], variables[idx].str_val, MAX_STRING_LEN);
            } else {
                f->saved_int_vals[i] = variables[idx].int_val;
            }

            // Set new value from argument
            skip_spaces();
            if (s->param_is_string[i]) {
                char tmp[MAX_STRING_LEN];
                str_expr(tmp);
                str_copy(variables[idx].str_val, tmp, MAX_STRING_LEN);
            } else {
                variables[idx].int_val = expr();
            }
        }
        skip_spaces();
        if (*ptr == ',') ptr++;
    }

    skip_spaces();
    if (*ptr == ')') ptr++;

    call_sp++;

    // Save control flow stack states for nested calls
    int saved_if_sp = if_sp;
    int saved_for_sp = for_sp;
    int saved_while_sp = while_sp;
    int saved_do_sp = do_sp;
    int saved_select_sp = select_sp;

    // Execute the SUB/FUNCTION body
    int saved_line = current_line;
    current_line = s->start_line + 1;  // First line after SUB/FUNCTION

    while (running && current_line < num_lines) {
        const char *p = program[current_line];
        while (*p == ' ' || *p == '\t') p++;

        // Check for END SUB / END FUNCTION
        if (to_upper(p[0]) == 'E' && to_upper(p[1]) == 'N' &&
            to_upper(p[2]) == 'D' && (p[3] == ' ' || p[3] == '\t')) {
            const char *q = p + 4;
            while (*q == ' ' || *q == '\t') q++;
            if ((to_upper(q[0]) == 'S' && to_upper(q[1]) == 'U' &&
                 to_upper(q[2]) == 'B' && !is_alpha(q[3])) ||
                (to_upper(q[0]) == 'F' && to_upper(q[1]) == 'U' &&
                 to_upper(q[2]) == 'N' && to_upper(q[3]) == 'C' &&
                 to_upper(q[4]) == 'T' && to_upper(q[5]) == 'I' &&
                 to_upper(q[6]) == 'O' && to_upper(q[7]) == 'N' && !is_alpha(q[8]))) {
                // End of this SUB/FUNCTION
                break;
            }
        }

        execute_line(program[current_line]);
        current_line++;
    }

    // Get return value (for FUNCTION)
    int32_t ret_val = 0;
    if (s->is_function && call_sp > 0) {
        f = &call_stack[call_sp - 1];
        ret_val = f->func_return_val;
        if (return_str && str_result) {
            str_copy(str_result, f->func_return_str, MAX_STRING_LEN);
        }
    }

    // Pop call frame and restore parameters
    if (call_sp > 0) {
        call_sp--;
        f = &call_stack[call_sp];
        for (int i = 0; i < s->num_params; i++) {
            int idx = find_var(s->params[i], s->param_is_string[i]);
            if (idx >= 0) {
                if (s->param_is_string[i]) {
                    str_copy(variables[idx].str_val, f->saved_str_vals[i], MAX_STRING_LEN);
                } else {
                    variables[idx].int_val = f->saved_int_vals[i];
                }
            }
        }
    }

    // Restore control flow stack states
    if_sp = saved_if_sp;
    for_sp = saved_for_sp;
    while_sp = saved_while_sp;
    do_sp = saved_do_sp;
    select_sp = saved_select_sp;

    current_line = saved_line;
    return ret_val;
}

// CALL name(args)
static void stmt_call(void) {
    skip_spaces();
    char name[MAX_VAR_NAME];
    int ni = 0;
    while ((is_alpha(*ptr) || is_digit(*ptr)) && ni < MAX_VAR_NAME - 1) {
        name[ni++] = to_upper(*ptr++);
    }
    name[ni] = '\0';

    int sub_idx = find_sub(name);
    if (sub_idx < 0) { error("SUB NOT FOUND"); return; }

    call_sub_or_func(sub_idx, 0, 0);
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

// LOCATE row, col (1-based)
static void stmt_locate(void) {
    skip_spaces();
    int32_t row = expr();
    skip_spaces();
    if (*ptr == ',') ptr++;
    int32_t col = expr();
    display_set_cursor(col - 1, row - 1);
    print_col = col - 1;  // Sync PRINT column tracker
}

// COLOR fg [, bg]
static void stmt_color(void) {
    skip_spaces();
    int32_t fg = expr();
    int32_t bg = 0;  // Default: black background
    skip_spaces();
    if (*ptr == ',') {
        ptr++;
        bg = expr();
    }
    display_set_color((uint8_t)fg, (uint8_t)bg);
}

// RANDOMIZE [seed]
static void stmt_randomize(void) {
    skip_spaces();
    if (*ptr && *ptr != ':' && *ptr != '\'') {
        rng_state = (uint32_t)expr();
    } else {
        rng_state = 12345;  // Default seed
    }
}

// SWAP var1, var2
static void stmt_swap(void) {
    skip_spaces();
    char name1[MAX_VAR_NAME], name2[MAX_VAR_NAME];
    int is_str1 = parse_var_name(name1);
    int idx1 = get_or_create_var(name1, is_str1);
    skip_spaces();
    if (*ptr == ',') ptr++;
    skip_spaces();
    int is_str2 = parse_var_name(name2);
    int idx2 = get_or_create_var(name2, is_str2);
    if (idx1 < 0 || idx2 < 0 || is_str1 != is_str2) return;
    Variable *v1 = &variables[idx1], *v2 = &variables[idx2];
    if (is_str1) {
        char tmp[MAX_STRING_LEN];
        str_copy(tmp, v1->str_val, MAX_STRING_LEN);
        str_copy(v1->str_val, v2->str_val, MAX_STRING_LEN);
        str_copy(v2->str_val, tmp, MAX_STRING_LEN);
    } else {
        int32_t tmp = v1->int_val;
        v1->int_val = v2->int_val;
        v2->int_val = tmp;
    }
}

// SLEEP n (n seconds, busy-wait)
static void stmt_sleep(void) {
    skip_spaces();
    int32_t n = expr();
    (void)n;  // In emulator, just continue (no real delay)
}

// BEEP
static void stmt_beep(void) {
    // No audio in emulator, just ignore
}

// ERASE arrayname
static void stmt_erase(void) {
    skip_spaces();
    char name[MAX_VAR_NAME];
    int is_string = parse_var_name(name);
    int idx = find_var(name, is_string);
    if (idx < 0) return;
    Variable *v = &variables[idx];
    if (!v->is_array) return;
    // Reset array to zeros/empty strings
    for (int i = 0; i < v->array_size; i++) {
        if (is_string) v->str_array[i][0] = '\0';
        else v->int_array[i] = 0;
    }
}

// LINE INPUT [prompt;] var$
static void stmt_line_input(void) {
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
    if (!is_string) return;  // LINE INPUT only for strings
    int idx = get_or_create_var(name, 1);
    if (idx < 0) return;
    Variable *v = &variables[idx];
    // Read entire line including commas
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
    str_copy(v->str_val, buf, MAX_STRING_LEN);
}

// Check if at end of statement (for block IF detection)
static int is_end_of_stmt(void) {
    const char *p = ptr;
    while (*p == ' ' || *p == '\t') p++;
    return *p == '\0' || *p == ':' || *p == '\'';
}

// Skip to matching ELSEIF, ELSE, or END IF at current nesting level
// mode: 0 = stop at ELSEIF/ELSE/ENDIF, 1 = stop only at ENDIF
static void skip_to_else_or_endif(int mode) {
    int depth = 1;
    while (depth > 0 && current_line < num_lines - 1) {
        current_line++;
        const char *p = program[current_line];
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;

            // Check for IF (start of block - need to check for THEN at end)
            if (to_upper(p[0]) == 'I' && to_upper(p[1]) == 'F' && !is_alpha(p[2])) {
                // Scan to see if this is a block IF (THEN at end of line)
                const char *q = p + 2;
                while (*q && *q != '\'' && *q != ':') {
                    if (to_upper(q[0]) == 'T' && to_upper(q[1]) == 'H' &&
                        to_upper(q[2]) == 'E' && to_upper(q[3]) == 'N' && !is_alpha(q[4])) {
                        q += 4;
                        while (*q == ' ' || *q == '\t') q++;
                        if (*q == '\0' || *q == '\'' || *q == ':') {
                            depth++;  // Block IF
                        }
                        break;
                    }
                    q++;
                }
            }
            // Check for END IF
            else if (to_upper(p[0]) == 'E' && to_upper(p[1]) == 'N' && to_upper(p[2]) == 'D' &&
                     (p[3] == ' ' || p[3] == '\t')) {
                const char *q = p + 4;
                while (*q == ' ' || *q == '\t') q++;
                if (to_upper(q[0]) == 'I' && to_upper(q[1]) == 'F' && !is_alpha(q[2])) {
                    depth--;
                    if (depth == 0) return;
                }
            }
            // Check for ELSEIF (only at depth 1, mode 0)
            else if (depth == 1 && mode == 0 &&
                     to_upper(p[0]) == 'E' && to_upper(p[1]) == 'L' &&
                     to_upper(p[2]) == 'S' && to_upper(p[3]) == 'E' &&
                     to_upper(p[4]) == 'I' && to_upper(p[5]) == 'F' && !is_alpha(p[6])) {
                current_line--;  // Back up so execute_line processes ELSEIF
                return;
            }
            // Check for ELSE (only at depth 1, mode 0)
            else if (depth == 1 && mode == 0 &&
                     to_upper(p[0]) == 'E' && to_upper(p[1]) == 'L' &&
                     to_upper(p[2]) == 'S' && to_upper(p[3]) == 'E' && !is_alpha(p[4])) {
                current_line--;  // Back up so execute_line processes ELSE
                return;
            }

            while (*p && *p != ':') p++;
            if (*p == ':') p++;
        }
    }
}

static void stmt_if(void) {
    int32_t cond = expr();
    skip_spaces();

    if (!match_keyword("THEN")) { error("EXPECTED THEN"); return; }

    skip_spaces();
    if (is_end_of_stmt()) {
        // Block IF
        if (if_sp >= MAX_STACK) { error("IF OVERFLOW"); return; }
        if_stack[if_sp].branch_taken = cond ? 1 : 0;
        if_sp++;

        if (!cond) {
            skip_to_else_or_endif(0);
        }
    } else {
        // Single-line IF
        if (cond) {
            if (is_digit(*ptr)) {
                int linenum = parse_number();
                int idx = find_line(linenum);
                if (idx >= 0) current_line = idx - 1;
            } else {
                execute_line(ptr);
            }
        } else {
            // Skip to ELSE if present
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
}

static void stmt_elseif(void) {
    if (if_sp <= 0) { error("ELSEIF WITHOUT IF"); return; }

    IfFrame *f = &if_stack[if_sp - 1];
    if (f->branch_taken) {
        // Already took a branch, skip to END IF
        skip_to_else_or_endif(1);
        return;
    }

    int32_t cond = expr();
    skip_spaces();
    if (!match_keyword("THEN")) { error("EXPECTED THEN"); return; }

    if (cond) {
        f->branch_taken = 1;
    } else {
        skip_to_else_or_endif(0);
    }
}

static void stmt_else(void) {
    if (if_sp <= 0) { error("ELSE WITHOUT IF"); return; }

    IfFrame *f = &if_stack[if_sp - 1];
    if (f->branch_taken) {
        // Already took a branch, skip to END IF
        skip_to_else_or_endif(1);
    }
    // Otherwise just continue executing
}

static void stmt_endif(void) {
    if (if_sp <= 0) { error("END IF WITHOUT IF"); return; }
    if_sp--;
}

// Skip to matching CASE, CASE ELSE, or END SELECT
// mode: 0 = stop at any, 1 = stop only at END SELECT
static void skip_to_case_or_end_select(int mode) {
    int depth = 1;
    while (depth > 0 && current_line < num_lines - 1) {
        current_line++;
        const char *p = program[current_line];
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;

            // Check for SELECT CASE (nested)
            if (to_upper(p[0]) == 'S' && to_upper(p[1]) == 'E' &&
                to_upper(p[2]) == 'L' && to_upper(p[3]) == 'E' &&
                to_upper(p[4]) == 'C' && to_upper(p[5]) == 'T' && !is_alpha(p[6])) {
                depth++;
            }
            // Check for END SELECT
            else if (to_upper(p[0]) == 'E' && to_upper(p[1]) == 'N' &&
                     to_upper(p[2]) == 'D' && (p[3] == ' ' || p[3] == '\t')) {
                const char *q = p + 4;
                while (*q == ' ' || *q == '\t') q++;
                if (to_upper(q[0]) == 'S' && to_upper(q[1]) == 'E' &&
                    to_upper(q[2]) == 'L' && to_upper(q[3]) == 'E' &&
                    to_upper(q[4]) == 'C' && to_upper(q[5]) == 'T' && !is_alpha(q[6])) {
                    depth--;
                    if (depth == 0) return;
                }
            }
            // Check for CASE (only at depth 1, mode 0)
            else if (depth == 1 && mode == 0 &&
                     to_upper(p[0]) == 'C' && to_upper(p[1]) == 'A' &&
                     to_upper(p[2]) == 'S' && to_upper(p[3]) == 'E' && !is_alpha(p[4])) {
                current_line--;  // Back up so execute_line processes CASE
                return;
            }

            while (*p && *p != ':') p++;
            if (*p == ':') p++;
        }
    }
}

static void stmt_select_case(void) {
    if (select_sp >= MAX_STACK) { error("SELECT OVERFLOW"); return; }

    skip_spaces();
    SelectFrame *f = &select_stack[select_sp];

    // Determine if string or integer expression
    if (is_string_expr()) {
        f->is_string = 1;
        str_expr(f->str_val);
    } else {
        f->is_string = 0;
        f->int_val = expr();
    }
    f->case_matched = 0;
    select_sp++;

    // Skip to first CASE
    skip_to_case_or_end_select(0);
}

// String comparison helper
static int str_compare(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void stmt_case(void) {
    if (select_sp <= 0) { error("CASE WITHOUT SELECT"); return; }

    SelectFrame *f = &select_stack[select_sp - 1];

    // If already matched a case, skip to END SELECT
    if (f->case_matched) {
        skip_to_case_or_end_select(1);
        return;
    }

    skip_spaces();

    // Check for CASE ELSE
    if (match_keyword("ELSE")) {
        f->case_matched = 1;
        return;  // Execute the ELSE block
    }

    // Check values in CASE clause
    int matched = 0;
    while (!matched && *ptr && *ptr != ':' && *ptr != '\'') {
        skip_spaces();

        if (f->is_string) {
            // String comparison
            char case_val[MAX_STRING_LEN];
            str_expr(case_val);
            if (str_compare(f->str_val, case_val) == 0) {
                matched = 1;
            }
        } else {
            // Integer comparison
            int32_t val1 = expr();
            skip_spaces();

            // Check for range (TO)
            if (match_keyword("TO")) {
                int32_t val2 = expr();
                if (f->int_val >= val1 && f->int_val <= val2) {
                    matched = 1;
                }
            } else {
                if (f->int_val == val1) {
                    matched = 1;
                }
            }
        }

        skip_spaces();
        if (*ptr == ',') {
            ptr++;
        } else {
            break;
        }
    }

    if (matched) {
        f->case_matched = 1;
    } else {
        skip_to_case_or_end_select(0);
    }
}

static void stmt_end_select(void) {
    if (select_sp <= 0) { error("END SELECT WITHOUT SELECT"); return; }
    select_sp--;
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
        else if (match_keyword("DO")) { stmt_do(); return; }
        else if (match_keyword("LOOP")) { stmt_loop(); return; }
        else if (match_keyword("EXIT")) {
            skip_spaces();
            if (match_keyword("DO")) { stmt_exit_do(); return; }
            else if (match_keyword("FOR")) { stmt_exit_for(); return; }
            else { error("EXPECTED DO OR FOR"); return; }
        }
        else if (match_keyword("ELSEIF")) { stmt_elseif(); return; }
        else if (match_keyword("ELSE")) { stmt_else(); return; }
        else if (match_keyword("SELECT")) {
            skip_spaces();
            if (match_keyword("CASE")) { stmt_select_case(); return; }
        }
        else if (match_keyword("CASE")) { stmt_case(); return; }
        else if (match_keyword("IF")) { stmt_if(); return; }
        else if (match_keyword("ON")) { stmt_on(); return; }
        else if (match_keyword("DECLARE")) { stmt_declare(); }
        else if (match_keyword("SUB")) { stmt_sub_def(); return; }
        else if (match_keyword("FUNCTION")) { stmt_func_def(); return; }
        else if (match_keyword("CALL")) { stmt_call(); }
        else if (to_upper(ptr[0]) == 'E' && to_upper(ptr[1]) == 'N' && to_upper(ptr[2]) == 'D' &&
                 ptr[3] == ' ') {
            ptr += 4; skip_spaces();
            if (match_keyword("IF")) { stmt_endif(); }
            else if (match_keyword("SELECT")) { stmt_end_select(); }
            else if (match_keyword("SUB")) { return; }  // END SUB handled by call_sub_or_func
            else if (match_keyword("FUNCTION")) { return; }  // END FUNCTION handled by call_sub_or_func
            else { running = 0; return; }  // END without IF/SELECT = END program
        }
        else if (match_keyword("READ")) stmt_read();
        else if (match_keyword("RESTORE")) stmt_restore();
        else if (match_keyword("CLS")) stmt_cls();
        else if (match_keyword("PSET")) stmt_pset();
        else if (match_keyword("LINE")) {
            if (match_keyword("INPUT")) stmt_line_input();
            else stmt_line();
        }
        else if (match_keyword("CIRCLE")) stmt_circle();
        else if (match_keyword("FCIRCLE")) stmt_fcircle();
        else if (match_keyword("PAINT")) stmt_paint();
        else if (match_keyword("LOCATE")) stmt_locate();
        else if (match_keyword("COLOR")) stmt_color();
        else if (match_keyword("RANDOMIZE")) stmt_randomize();
        else if (match_keyword("SWAP")) stmt_swap();
        else if (match_keyword("SLEEP")) stmt_sleep();
        else if (match_keyword("BEEP")) stmt_beep();
        else if (match_keyword("ERASE")) stmt_erase();
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
    gosub_sp = for_sp = while_sp = do_sp = if_sp = select_sp = call_sp = 0;
    data_line = 0; data_ptr = 0;
    current_line = 0;
    running = 1;
    scan_subs();  // Scan for SUB/FUNCTION definitions

    while (running && current_line < num_lines) {
        jump_pending = 0;
        execute_line(program[current_line]);
        if (!jump_pending) current_line++;
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
