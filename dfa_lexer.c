#include <stdio.h>
#include <string.h>
#include "input_system.h"

/* ════════════════════════════════════════════════════════════
   TOKEN TYPES
   ════════════════════════════════════════════════════════════ */
typedef enum {
    TOK_HEADER     =  0,
    TOK_COMMENT    =  1,
    TOK_TYPE_INT   =  2,
    TOK_TYPE_DEC   =  3,
    TOK_VAR_NAME   =  4,
    TOK_FUNC_NAME  =  5,
    TOK_LOOP_LABEL =  6,
    TOK_STMT_END   =  7,
    TOK_NUMBER     =  8,
    TOK_SYMBOL     =  9,
    TOK_UNKNOWN    = 10
} TokenType;

/* parallel to the enum — index IS the token type */
static const char *token_label[] = {
    "HEADER",
    "COMMENT",
    "TYPE:int",
    "TYPE:dec",
    "VAR_NAME",
    "FUNC_NAME",
    "LOOP_LABEL",
    "STMT_END",
    "NUMBER",
    "SYMBOL",
    "UNKNOWN"
};

/* ════════════════════════════════════════════════════════════
   MASTER DFA STATE ENUMERATION
   ════════════════════════════════════════════════════════════

   Every token class gets its own sub-path through the DFA.
   State names encode what has been consumed so far.

   HEADER path : H_*
     #include<stdio.h>  — matched character-by-character as
     a straight chain; only one possible lexeme so we treat
     each character position as a state.

   COMMENT path : CM_*
     //  followed by zero-or-more (alpha | space)

   NUMBER path : NM_*
     one or more digits

   SYMBOL path : SY_*
     single character  ( ) [ ] < >

   STMT_END path : SE_*
     ..   (two dots)

   VAR_NAME path : VA_*
     _  [alpha]+  [digit]  [alpha]

   FUNC_NAME path : FN_*
     [alpha]+  ending in 'F' 'n'
     (shares alpha states with keyword / type paths)

   TYPE / KEYWORD path : KW_*
     "int"  →  TOK_TYPE_INT
     "dec"  →  TOK_TYPE_DEC

   LOOP_LABEL path : LL_*
     loop_  [alpha]+  [digit][digit]  :

   DEAD : absorbing error state
   ════════════════════════════════════════════════════════════ */
typedef enum {

    /* ── start ─────────────────────────────────── */
    S_START = 0,

    /* ── HEADER:  #include<stdio.h> ────────────── */
    H_HASH,          /*  #                  */
    H_I,             /*  #i                 */
    H_N,             /*  #in                */
    H_C,             /*  #inc               */
    H_L,             /*  #incl              */
    H_U,             /*  #inclu             */
    H_D,             /*  #includ            */
    H_E1,            /*  #include           */
    H_LT,            /*  #include<          */
    H_S,             /*  #include<s         */
    H_T,             /*  #include<st        */
    H_DI,            /*  #include<sti       */
    H_O,             /*  #include<stdi      */
    H_DOT,           /*  #include<stdio     */
    H_H,             /*  #include<stdio.    */
    H_GT,            /*  #include<stdio.h   */
    H_DONE,          /*  #include<stdio.h>  ← ACCEPT */

    /* ── COMMENT:  // alpha* ────────────────────── */
    CM_SLASH1,       /*  /                  */
    CM_SLASH2,       /*  //                 ← ACCEPT (empty) */
    CM_BODY,         /*  // chars           ← ACCEPT */

    /* ── STMT_END:  .. ──────────────────────────── */
    SE_DOT1,         /*  .                  */
    SE_DOT2,         /*  ..                 ← ACCEPT */

    /* ── NUMBER:  [0-9]+ ────────────────────────── */
    NM_DIGIT,        /*  one or more digits ← ACCEPT */

    /* ── SYMBOL:  single ( ) [ ] < > ───────────── */
    SY_ONE,          /*  one symbol char    ← ACCEPT */

    /* ── KEYWORD / TYPE shared prefix ──────────── */
    /*   "int"   */
    KW_I,            /*  i                  */
    KW_IN,           /*  in                 */
    KW_INT,          /*  int                ← ACCEPT TYPE_INT */
    /*   "dec"   */
    KW_D,            /*  d                  */
    KW_DE,           /*  de                 */
    KW_DEC,          /*  dec                ← ACCEPT TYPE_DEC */

    /* ── FUNC_NAME:  [alpha]+ ending Fn ─────────── */
    /*   any alpha word that doesn't match a keyword
         lives in these states until it ends in Fn    */
    FN_ALPHA,        /*  one+ alpha (generic)          */
    FN_F,            /*  last char seen was 'F'         */
    FN_DONE,         /*  last two chars were 'Fn' ← ACCEPT */

    /* ── VAR_NAME:  _ [alpha]+ [digit] [alpha] ─── */
    VA_UNDER,        /*  _                  */
    VA_ALPHA,        /*  _ alpha+           */
    VA_DIGIT,        /*  _ alpha+ digit     */
    VA_DONE,         /*  _ alpha+ digit alpha ← ACCEPT */

    /* ── LOOP_LABEL:  loop_ [alpha]+ [d][d] : ─── */
    LL_L,            /*  l                  */
    LL_LO,           /*  lo                 */
    LL_LOO,          /*  loo                */
    LL_LOOP,         /*  loop               */
    LL_LOOP_,        /*  loop_              */
    LL_BODY,         /*  loop_ alpha+       */
    LL_D1,           /*  loop_ alpha+ digit */
    LL_D2,           /*  loop_ alpha+ digit digit */
    LL_DONE,         /*  loop_ alpha+ digit digit : ← ACCEPT */

    /* ── dead / error ───────────────────────────── */
    S_DEAD,

    NUM_STATES       /* must be last */
} State;

/* ════════════════════════════════════════════════════════════
   ACCEPTING STATE TABLE
   Index = State enum value.
   Value = TokenType if accepting, -1 if not.
   No if-else needed: result = accept_token[final_state]
   and if result >= 0 it IS the token type.
   ════════════════════════════════════════════════════════════ */
static const int accept_token[] = {
    /* S_START    */ -1,
    /* H_HASH     */ -1,
    /* H_I        */ -1,
    /* H_N        */ -1,
    /* H_C        */ -1,
    /* H_L        */ -1,
    /* H_U        */ -1,
    /* H_D        */ -1,
    /* H_E1       */ -1,
    /* H_LT       */ -1,
    /* H_S        */ -1,
    /* H_T        */ -1,
    /* H_DI       */ -1,
    /* H_O        */ -1,
    /* H_DOT      */ -1,
    /* H_H        */ -1,
    /* H_GT       */ -1,
    /* H_DONE     */ TOK_HEADER,
    /* CM_SLASH1  */ -1,
    /* CM_SLASH2  */ TOK_COMMENT,
    /* CM_BODY    */ TOK_COMMENT,
    /* SE_DOT1    */ -1,
    /* SE_DOT2    */ TOK_STMT_END,
    /* NM_DIGIT   */ TOK_NUMBER,
    /* SY_ONE     */ TOK_SYMBOL,
    /* KW_I       */ -1,
    /* KW_IN      */ -1,
    /* KW_INT     */ TOK_TYPE_INT,
    /* KW_D       */ -1,
    /* KW_DE      */ -1,
    /* KW_DEC     */ TOK_TYPE_DEC,
    /* FN_ALPHA   */ -1,
    /* FN_F       */ -1,
    /* FN_DONE    */ TOK_FUNC_NAME,
    /* VA_UNDER   */ -1,
    /* VA_ALPHA   */ -1,
    /* VA_DIGIT   */ -1,
    /* VA_DONE    */ TOK_VAR_NAME,
    /* LL_L       */ -1,
    /* LL_LO      */ -1,
    /* LL_LOO     */ -1,
    /* LL_LOOP    */ -1,
    /* LL_LOOP_   */ -1,
    /* LL_BODY    */ -1,
    /* LL_D1      */ -1,
    /* LL_D2      */ -1,
    /* LL_DONE    */ TOK_LOOP_LABEL,
    /* S_DEAD     */ -1
};

/* ════════════════════════════════════════════════════════════
   MASTER TRANSITION TABLE
   next_state[current_state][input_id]

   Rows  = State enum  (NUM_STATES rows)
   Cols  = InputID enum (NUM_INPUTS = 14 cols)

   Column order (must match InputID enum in input_system.h):
     0  IN_HASH
     1  IN_SLASH
     2  IN_DOT
     3  IN_UNDER
     4  IN_UPPER_F
     5  IN_LOWER_N
     6  IN_LOWER_L
     7  IN_LOWER_O
     8  IN_LOWER_P
     9  IN_COLON
    10  IN_ALPHA
    11  IN_DIGIT
    12  IN_SYMBOL
    13  IN_OTHER
   ════════════════════════════════════════════════════════════ */

/* shorthand to keep the table readable */
#define DD S_DEAD
#define __ S_DEAD   /* unused / forbidden transition */

static const State next_state[NUM_STATES][NUM_INPUTS] = {
/*               #       /       .       _       F       n       l       o       p       :      alpha  digit  sym   other */
/* S_START  */{ H_HASH, CM_SLASH1,SE_DOT1,VA_UNDER,FN_F,  KW_IN+1,LL_L,  FN_ALPHA,FN_ALPHA,DD, FN_ALPHA,NM_DIGIT,SY_ONE,DD },

/* H_HASH   */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_I,    DD,     DD,   DD },
/* H_I      */{ DD,     DD,      DD,     DD,     DD,     H_N,    DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },
/* H_N      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_C,    DD,     DD,   DD },
/* H_C      */{ DD,     DD,      DD,     DD,     DD,     DD,     H_L,    DD,     DD,     DD,     DD,     DD,     DD,   DD },
/* H_L      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_U,    DD,     DD,   DD },
/* H_U      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_D,    DD,     DD,   DD },
/* H_D      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_E1,   DD,     DD,   DD },
/* H_E1     */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_LT, DD },
/* H_LT     */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_S,    DD,     DD,   DD },
/* H_S      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_T,    DD,     DD,   DD },
/* H_T      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_DI,   DD,     DD,   DD },
/* H_DI     */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     H_O,    DD,     DD,     DD,     DD,     DD,   DD },
/* H_O      */{ DD,     DD,      H_DOT,  DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },
/* H_DOT    */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_H,    DD,     DD,   DD },
/* H_H      */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     H_GT, DD },
/* H_GT     */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   H_DONE },
/* H_DONE   */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },

/* CM_SLASH1*/{ DD,     CM_SLASH2,DD,    DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },
/* CM_SLASH2*/{ DD,     DD,      DD,     DD,     CM_BODY,CM_BODY,CM_BODY,CM_BODY,CM_BODY,DD,    CM_BODY, DD,    DD,   CM_BODY},
/* CM_BODY  */{ DD,     DD,      DD,     DD,     CM_BODY,CM_BODY,CM_BODY,CM_BODY,CM_BODY,DD,    CM_BODY, DD,    DD,   CM_BODY},

/* SE_DOT1  */{ DD,     DD,      SE_DOT2,DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },
/* SE_DOT2  */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },

/* NM_DIGIT */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     NM_DIGIT,DD,  DD },

/* SY_ONE   */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },

/* KW_I     */{ DD,     DD,      DD,     DD,     FN_F,   KW_IN,  FN_ALPHA,FN_ALPHA,FN_ALPHA,DD, FN_ALPHA,DD,    DD,   DD },
/* KW_IN    */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,KW_INT, DD,     DD,   DD },
/* KW_INT   */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,    DD,   DD },

/* KW_D     */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,    DD,   DD },
/* KW_DE    */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,KW_DEC, DD,     DD,   DD },
/* KW_DEC   */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,    DD,   DD },

/* FN_ALPHA */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,    DD,   DD },
/* FN_F     */{ DD,     DD,      DD,     DD,     FN_F,   FN_DONE, FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,    DD,   DD },
/* FN_DONE  */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,    DD,   DD },

/* VA_UNDER */{ DD,     DD,      DD,     DD,     VA_ALPHA,VA_ALPHA,VA_ALPHA,VA_ALPHA,VA_ALPHA,DD,VA_ALPHA,DD,    DD,   DD },
/* VA_ALPHA */{ DD,     DD,      DD,     DD,     VA_ALPHA,VA_ALPHA,VA_ALPHA,VA_ALPHA,VA_ALPHA,DD,VA_ALPHA,VA_DIGIT,DD, DD },
/* VA_DIGIT */{ DD,     DD,      DD,     DD,     VA_DONE, VA_DONE, VA_DONE, VA_DONE, VA_DONE,DD, VA_DONE, DD,   DD,   DD },
/* VA_DONE  */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },

/* LL_L     */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,LL_LO,  FN_ALPHA,DD, FN_ALPHA,DD,   DD,   DD },
/* LL_LO    */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,LL_LOO, FN_ALPHA,DD, FN_ALPHA,DD,   DD,   DD },
/* LL_LOO   */{ DD,     DD,      DD,     DD,     FN_F,   FN_ALPHA,FN_ALPHA,FN_ALPHA,LL_LOOP,DD, FN_ALPHA,DD,   DD,   DD },
/* LL_LOOP  */{ DD,     DD,      DD,     LL_LOOP_,FN_F,  FN_ALPHA,FN_ALPHA,FN_ALPHA,FN_ALPHA,DD,FN_ALPHA,DD,   DD,   DD },
/* LL_LOOP_ */{ DD,     DD,      DD,     DD,     LL_BODY,LL_BODY, LL_BODY, LL_BODY, LL_BODY,DD, LL_BODY, DD,   DD,   DD },
/* LL_BODY  */{ DD,     DD,      DD,     DD,     LL_BODY,LL_BODY, LL_BODY, LL_BODY, LL_BODY,DD, LL_BODY, LL_D1,DD,  DD },
/* LL_D1    */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     LL_D2,  DD,   DD },
/* LL_D2    */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     LL_DONE,DD,     DD,     DD,   DD },
/* LL_DONE  */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD },

/* S_DEAD   */{ DD,     DD,      DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,     DD,   DD }
};

#undef DD
#undef __

/* ════════════════════════════════════════════════════════════
   run_dfa  — identical to the lab's main loop
   Feeds one lexeme through the master DFA.
   Returns the TokenType or TOK_UNKNOWN.
   Zero if-else, zero strcmp.
   ════════════════════════════════════════════════════════════ */
static TokenType run_dfa(const char *lexeme) {
    State state = S_START;

    for (int i = 0; lexeme[i] != '\0'; i++) {
        InputID inp = get_input(lexeme[i]);
        state = next_state[state][inp];

        /* S_DEAD is absorbing — stop early */
        static const int is_dead[NUM_STATES] = {
            [S_DEAD] = 1
        };
        if (is_dead[state]) break;
    }

    /* accept_token[] maps state → TokenType (-1 = unknown) */
    int tok = accept_token[state];
    /* branchless clamp: if tok < 0 return UNKNOWN, else tok */
    return (TokenType)( (tok & ~(tok >> 31)) |
                        (TOK_UNKNOWN & (tok >> 31)) );
}

/* ════════════════════════════════════════════════════════════
   TOKENISER
   Scans source line-by-line, extracts raw lexemes,
   then calls run_dfa() — the only classification step.
   ════════════════════════════════════════════════════════════ */

/* Special 2-char and 1-char splitters that need early detection
   before the general word collector runs.                    */
static void emit(const char *lex, int line_no) {
    TokenType tok = run_dfa(lex);
    printf("  %-24s  %-14s  line %d\n",
           lex, token_labels[tok], line_no);
}

void tokenize(const char *source) {
    char line[512], buf[256];
    int  line_no = 0;
    const char *p = source;

    printf("\n┌─────────────────────────────────────────────────┐\n");
    printf("│            LEXICAL ANALYSIS OUTPUT             │\n");
    printf("├──────────────────────────┬──────────────┬──────┤\n");
    printf("│ LEXEME                   │ TOKEN        │ LINE │\n");
    printf("├──────────────────────────┼──────────────┼──────┤\n");

    while (*p) {
        /* collect one line */
        int li = 0;
        while (*p && *p != '\n') line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        line_no++;

        int i = 0;
        while (line[i]) {
            unsigned char ch = (unsigned char)line[i];

            /* ── whitespace ── */
            if (ch == ' ' || ch == '\t') { i++; continue; }

            /* ── comment: // consumes rest of line ── */
            if (ch == '/' && line[i+1] == '/') {
                int bi = 0;
                while (line[i]) buf[bi++] = line[i++];
                buf[bi] = '\0';
                emit(buf, line_no);
                break;
            }

            /* ── ".." two-char token ── */
            if (ch == '.' && line[i+1] == '.') {
                buf[0]='.'; buf[1]='.'; buf[2]='\0';
                emit(buf, line_no);
                i += 2; continue;
            }

            /* ── single-char symbol ── */
            if (get_input(ch) == IN_SYMBOL) {
                buf[0] = ch; buf[1] = '\0';
                emit(buf, line_no);
                i++; continue;
            }

            /* ── general lexeme: collect until delimiter ── */
            {
                int bi = 0;
                while (line[i]
                    && line[i] != ' ' && line[i] != '\t'
                    && get_input((unsigned char)line[i]) != IN_SYMBOL
                    && !(line[i]=='.' && line[i+1]=='.'))
                {
                    buf[bi++] = line[i++];
                }
                buf[bi] = '\0';
                if (bi > 0) emit(buf, line_no);
            }
        }
    }
    printf("└──────────────────────────┴──────────────┴──────┘\n");
}


// project
/* ════════════════════════════════════════════════════════════
   MAIN  — reads filename from argv[1], falls back to test.txt
   ════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {

    /* choose file: argument or default */
    const char *filename = (argc >= 2) ? argv[1] : "test.txt";

    printf("Reading source file: %s\n", filename);

    char *source = read_file(filename);
    if (!source) return 1;

    tokenize(source);

    free(source);
    return 0;
}