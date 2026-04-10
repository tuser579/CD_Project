#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NUM_STATES 55
#define NUM_INPUTS 27

// DFA states mapped to exact rules
enum {
    S_START = 0,
    H1,                     // Header
    C1, C2,                 // Comments
    D1, D2,                 // Stmt End (..)
    V1, V2, V3, V_DONE,     // Variables (_var1a)
    F1, F_F, F_DONE,        // Functions (*Fn)
    I1, I2, I_DONE,         // int
    DEC1, DEC2, DEC_DONE,   // dec
    W1, W2, W3, W4, W_DONE, // while
    L1, L2, L3, L4, L5, L6, L7, L8, L_DONE, // loop label
    N1,                     // Numbers
    SYM,                    // Symbols
    R1, R2, R3, R4, R5, R_DONE, // return
    B1, B2, B3, B4, B_DONE,     // break
    CO1, CO2, CO3, CO4, CO5, CO6, CO7, CO_DONE, // continue
    S_DEAD                  // Dead state for token emission
};

// Map inputs to columns
int get_input(char ch) {
    switch(ch) {
        case '_': return 0;
        case 'F': return 1;
        case 'n': return 2;
        case 'l': return 3;
        case 'o': return 4;
        case 'p': return 5;
        case ':': return 6;
        case '.': return 7;
        case '/': return 8;
        case '#': return 9;
        case 'i': return 10;
        case 't': return 11;
        case 'd': return 12;
        case 'e': return 13;
        case 'c': return 14;
        case 'w': return 15;
        case 'h': return 16;
        case 'r': return 17;
        case 'u': return 18;
        case 'b': return 19;
        case 'a': return 20;
        case 'k': return 21;
        case '\0': case '\t': case '\r': return 25; // Nulls/Tabs (Space handling adjusted)
        case ' ': return 25; // SPACES
        case '\n': return 26; // NEWLINE
        case '<': case '>': case '(': case ')': case '[': case ']': case '=': return 24; // SYMBOL
    }
    if (isalpha((unsigned char)ch)) return 22; // ALPHABET
    if (isdigit((unsigned char)ch)) return 23; // DIGITS
    return 24; // Default SYMBOL
}

// Accepting states to string mapping
const char* accepting_tokens[NUM_STATES] = {
    [H1] = "HEADER",
    [C2] = "COMMENT",
    [D2] = "STMT_END",
    [V_DONE] = "VAR_NAME",
    [F_DONE] = "FUNC_NAME",
    [I_DONE] = "TYPE:int",
    [DEC_DONE] = "TYPE:dec",
    [W_DONE] = "KEYWORD",  
    [L_DONE] = "LOOP_LABEL",
    [N1] = "NUMBER",
    [SYM] = "SYMBOL",
    [R_DONE] = "KEYWORD",  
    [B_DONE] = "KEYWORD",
    [CO_DONE]= "KEYWORD"
};

// Parser storage
typedef struct {
    char lexeme[256];
    const char* token;
    int line;
} Token;

Token tokens[2000];
int token_count = 0;

// Read file helper
char *read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "Error: cannot open '%s'\n", path); return NULL; }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fprintf(stderr, "Error: out of memory\n"); fclose(fp); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, fp);
    buf[got] = '\0';
    fclose(fp);
    return buf;
}

// PARSER: Upgraded to check structural grammar rules
void parse_tokens() {
    printf("\n  ----------------------------------------------------------------\n");
    printf("  PARSER VERDICT\n");
    printf("  ----------------------------------------------------------------\n");

    if (token_count == 0) {
        printf("  Verdict: REJECTED (Empty program)\n\n");
        return;
    }
    
    // Rule: Must start with Header
    if (strcmp(tokens[0].token, "HEADER") != 0) {
        printf("  Verdict: REJECTED (Missing or invalid Header '#include<stdio.h>' at start)\n\n");
        return;
    }

    int has_main = 0;
    int reject = 0;
    int main_found = 0;

    for (int i = 0; i < token_count; i++) {
        if (strcmp(tokens[i].token, "UNKNOWN") == 0) {
            printf("  Error: Unrecognized/Invalid token '%s' at line %d\n", tokens[i].lexeme, tokens[i].line);
            reject = 1;
        }

        // Rule: Custom functions must be before MainFn
        if (strcmp(tokens[i].token, "FUNC_NAME") == 0) {
            if (strcmp(tokens[i].lexeme, "MainFn") == 0) {
                main_found = 1;
                has_main = 1;
            } else if (strcmp(tokens[i].lexeme, "printfFn") != 0) {
                if (main_found) {
                    printf("  Error: Custom function '%s' at line %d is placed after MainFn.\n", tokens[i].lexeme, tokens[i].line);
                    reject = 1;
                }
            }
        }

        // Rule: while loop exact syntax check 
        // [while] [TYPE] [VAR] [<] [NUMBER] [..]
        if (strcmp(tokens[i].token, "KEYWORD") == 0 && strcmp(tokens[i].lexeme, "while") == 0) {
            int valid_while = 1;
            if (i + 5 >= token_count) {
                valid_while = 0;
            } else {
                if (strncmp(tokens[i+1].token, "TYPE:", 5) != 0) valid_while = 0;
                if (strcmp(tokens[i+2].token, "VAR_NAME") != 0) valid_while = 0;
                if (strcmp(tokens[i+3].lexeme, "<") != 0) valid_while = 0;
                if (strcmp(tokens[i+4].token, "NUMBER") != 0) valid_while = 0;
                if (strcmp(tokens[i+5].token, "STMT_END") != 0) valid_while = 0;
            }
            if (!valid_while) {
                printf("  Error: Invalid 'while' loop syntax at line %d. Expected: while [type] [var] < [number] ..\n", tokens[i].line);
                reject = 1;
            }
        }
    }

    if (!has_main) {
        printf("  Error: Mandatory 'MainFn' function is missing.\n");
        reject = 1;
    }

    if (reject) {
        printf("  Verdict: REJECTED (Syntax/Lexical errors found)\n\n");
    } else {
        printf("  Verdict: ACCEPTED (Program conforms to grammatical structure)\n\n");
    }
}

int main(int argc, char *argv[]) {
    #define DD S_DEAD

    // Heavily tailored DFA. Notice changes to C2, V2, V3, L5, and F states.
    int next_state[NUM_STATES][NUM_INPUTS] = {
    /* _   F   n       l       o       p       :   .   /   #   i       t       d       e       c       w       h       r       u       b       a       k       AL      DG      SY  SP  NL */
    /* S_START*/{V1, F_F, F1,     L1,     F1,     F1,     SYM,D1, C1, H1, I1,     F1,     DEC1,   F1,     CO1,    W1,     F1,     R1,     F1,     B1,     F1,     F1,     F1,     N1,     SYM, DD, DD },
    /* H1     */{H1, H1,  H1,     H1,     H1,     H1,     H1, H1, H1, H1, H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,     H1,  DD, DD },
    /* C1     */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, C2, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    
    // COMMENT RULE: Ends on NL, accepts ONLY alphabet and spaces. No digits/symbols.
    /* C2     */{DD, C2,  C2,     C2,     C2,     C2,     DD, DD, DD, DD, C2,     C2,     C2,     C2,     C2,     C2,     C2,     C2,     C2,     C2,     C2,     C2,     C2,     DD,     DD,  C2, DD },
    
    /* D1     */{DD, DD,  DD,     DD,     DD,     DD,     DD, D2, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    /* D2     */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    
    // VAR RULE: _ -> alphabets -> exactly 1 digit -> exactly 1 alphabet.
    /* V1     */{DD, V2,  V2,     V2,     V2,     V2,     DD, DD, DD, DD, V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     DD,     DD,  DD, DD },
    /* V2     */{DD, V2,  V2,     V2,     V2,     V2,     DD, DD, DD, DD, V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V2,     V3,     DD,  DD, DD },
    /* V3     */{DD, V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,  DD, DD, DD, DD, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, V_DONE, DD,     DD,  DD, DD },
    /* V_DONE */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    
    // FUNC RULE: Strict alphabet check. Digits cause Dead State -> triggers Unknown Token.
    /* F1     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* F_F    */{DD, F_F, F_DONE, F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* F_DONE */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    
    /* I1     */{DD, F_F, I2,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* I2     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     I_DONE, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* I_DONE */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    
    /* DEC1   */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     DEC2,   F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* DEC2   */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     DEC_DONE,F1,    F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* DEC_DON*/{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    
    /* W1     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     W2,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* W2     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, W3,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* W3     */{DD, F_F, F1,     W4,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* W4     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     W_DONE, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* W_DONE */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    
    // LOOP LABEL: Extracted strict structure requirement for alphabet checking -> digit check
    /* L1     */{DD, F_F, F1,     F1,     L2,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* L2     */{DD, F_F, F1,     F1,     L3,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* L3     */{DD, F_F, F1,     F1,     F1,     L4,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* L4     */{L5, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* L5     */{DD, L6,  L6,     L6,     L6,     L6,     DD, DD, DD, DD, L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     DD,     DD,  DD, DD },
    /* L6     */{DD, L6,  L6,     L6,     L6,     L6,     DD, DD, DD, DD, L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L6,     L7,     DD,  DD, DD },
    /* L7     */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     L8,     DD,  DD, DD },
    /* L8     */{DD, DD,  DD,     DD,     DD,     DD,     L_DONE,DD,DD,DD,DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    /* L_DONE */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    
    /* N1     */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     N1,     DD,  DD, DD },
    /* SYM    */{DD, DD,  DD,     DD,     DD,     DD,     DD, DD, DD, DD, DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,  DD, DD },
    
    /* R1     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     R2,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* R2     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     R3,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* R3     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     R4,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* R4     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     R5,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* R5     */{DD, F_F, R_DONE, F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* R_DONE */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    
    /* B1     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     B2,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* B2     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     B3,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* B3     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     B4,     F1,     F1,     DD,     DD,  DD, DD },
    /* B4     */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     B_DONE, F1,     DD,     DD,  DD, DD },
    /* B_DONE */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    
    /* CO1    */{DD, F_F, F1,     F1,     CO2,    F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO2    */{DD, F_F, CO3,    F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO3    */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     CO4,    F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO4    */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, CO5,    F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO5    */{DD, F_F, CO6,    F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO6    */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     CO7,    F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO7    */{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     CO_DONE,F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD },
    /* CO_DONE*/{DD, F_F, F1,     F1,     F1,     F1,     DD, DD, DD, DD, F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     F1,     DD,     DD,  DD, DD }
    };
    #undef DD

    const char *filename = (argc >= 2) ? argv[1] : "test.txt";
    printf("Source file : %s\n", filename);
    char *source = read_file(filename);
    if (!source) return 1;

    printf("\n  %-30s  %-14s  %s\n", "LEXEME", "TOKEN", "LINE");
    printf("  ----------------------------------------------------------------\n");

    int state = S_START;
    char lexeme[256];
    int lex_len = 0;
    int line_no = 1;
    long source_len = strlen(source);

    for (long i = 0; i <= source_len; i++) {
        char ch = source[i];
        
        if (state == S_START && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\0')) {
            if (ch == '\n') line_no++;
            continue;
        }

        int input = get_input(ch);
        int next = next_state[state][input];

        if (next == S_DEAD) {
            if (state != S_START) {
                lexeme[lex_len] = '\0';
                const char* final_token = accepting_tokens[state] ? accepting_tokens[state] : "UNKNOWN";
                
                printf("  %-30s  %-14s  line %d\n", lexeme, final_token, line_no);
                
                if (token_count < 2000) {
                    strcpy(tokens[token_count].lexeme, lexeme);
                    tokens[token_count].token = final_token;
                    tokens[token_count].line = line_no;
                    token_count++;
                }

                state = S_START;
                lex_len = 0;
                i--; 
            } else {
                if (ch != '\0') {
                    printf("  %-30c  %-14s  line %d\n", ch, "UNKNOWN", line_no);
                }
            }
        } else {
            if (ch != '\0') {
                lexeme[lex_len++] = ch;
                state = next;
            }
        }
    }

    parse_tokens();
    free(source);
    return 0;
}