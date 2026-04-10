/*=============================================================
  lexer_parser.c  -  DFA Lexer + LL(1) Parser
  Language spec-compliant grammar.

  Output:
    SECTION 1 : LEXER TOKENISATION TABLE
    SECTION 2 : LL(1) PARSING TABLE  (step-by-step)
    SECTION 3 : VERDICT  (ACCEPTED / REJECTED)
=============================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
   SECTION 1 - DFA LEXER
   ============================================================ */

#define NUM_STATES  55
#define NUM_INPUTS  27
#define MAX_TOKENS  2000

/* ---- DFA States ---- */
enum {
    S_START=0,
    H1,
    C1,C2,
    D1,D2,
    V1,V2,V3,V_DONE,
    F1,F_F,F_DONE,
    I1,I2,I_DONE,
    DEC1,DEC2,DEC_DONE,
    W1,W2,W3,W4,W_DONE,
    L1,L2,L3,L4,L5,L6,L7,L8,L_DONE,
    N1,
    SYM,
    R1,R2,R3,R4,R5,R_DONE,
    B1,B2,B3,B4,B_DONE,
    CO1,CO2,CO3,CO4,CO5,CO6,CO7,CO_DONE,
    S_DEAD
};

/* ---- Character to input column (flat table, no if-else) ---- */
static int get_input(char ch) {
    static const struct { char c; int col; } MAP[] = {
        {'_',0},{'F',1},{'n',2},{'l',3},{'o',4},{'p',5},{':',6},
        {'.',7},{'/',8},{'#',9},{'i',10},{'t',11},{'d',12},{'e',13},
        {'c',14},{'w',15},{'h',16},{'r',17},{'u',18},{'b',19},
        {'a',20},{'k',21},
        {'\0',25},{'\t',25},{'\r',25},{' ',25},{'\n',26}
    };
    static const int MAP_LEN = (int)(sizeof MAP / sizeof MAP[0]);
    for (int i = 0; i < MAP_LEN; i++)
        if (MAP[i].c == ch) return MAP[i].col;
    if (isalpha((unsigned char)ch)) return 22;
    if (isdigit((unsigned char)ch)) return 23;
    return 24;
}

/* ---- Accepting state -> token name ---- */
static const char *ACCEPT[NUM_STATES] = {
    [H1]      ="HEADER",
    [C2]      ="COMMENT",
    [D2]      ="STMT_END",
    [V_DONE]  ="VAR_NAME",
    [F_DONE]  ="FUNC_NAME",
    [I_DONE]  ="TYPE:int",
    [DEC_DONE]="TYPE:dec",
    [W_DONE]  ="KEYWORD",
    [L_DONE]  ="LOOP_LABEL",
    [N1]      ="NUMBER",
    [SYM]     ="SYMBOL",
    [R_DONE]  ="KEYWORD",
    [B_DONE]  ="KEYWORD",
    [CO_DONE] ="KEYWORD"
};

/* ---- Lexeme store ---- */
typedef struct { char lexeme[256]; const char *token; int line; } LexToken;
static LexToken lex_tokens[MAX_TOKENS];
static int      lex_count = 0;

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr,"Cannot open '%s'\n", path); return NULL; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp); rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    buf[got] = '\0';
    fclose(fp);
    return buf;
}

/* DFA transition table [state][col]
   Cols: _  F  n  l  o  p  :  .  /  #  i  t  d  e  c  w  h  r  u  b  a  k  AL DG SY SP NL */
#define DD S_DEAD
static const int DFA[NUM_STATES][NUM_INPUTS] = {
/*S_START*/{V1, F_F,F1, L1, F1, F1, SYM,D1, C1, H1, I1, F1, DEC1,F1, CO1,W1, F1, R1, F1, B1, F1, F1, F1, N1, SYM,DD, DD},
/*H1     */{H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, H1, DD, DD},
/*C1     */{DD, DD, DD, DD, DD, DD, DD, DD, C2, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD},
/*C2     */{DD, C2, C2, C2, C2, C2, DD, DD, DD, DD, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, C2, DD, DD, C2, DD},
/*D1     */{DD, DD, DD, DD, DD, DD, DD, D2, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD},
/*D2     */{DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD},
/*V1     */{DD, V2, V2, V2, V2, V2, DD, DD, DD, DD, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, DD, DD, DD, DD},
/*V2     */{DD, V2, V2, V2, V2, V2, DD, DD, DD, DD, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V2, V3, DD, DD, DD},
/*V3     */{DD,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,DD,DD,DD,DD,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,V_DONE,DD,DD,DD,DD},
/*V_DONE */{DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD, DD},
/*F1     */{DD, F_F,F1, F1, F1, F1, DD, DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*F_F    */{DD, F_F,F_DONE,F1,F1,F1,DD,DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*F_DONE */{DD, F_F,F1, F1, F1, F1, DD, DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*I1     */{DD, F_F,I2, F1, F1, F1, DD, DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*I2     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,I_DONE,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*I_DONE */{DD, F_F,F1, F1, F1, F1, DD, DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*DEC1   */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,DEC2,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*DEC2   */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,DEC_DONE,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*DEC_DON*/{DD, F_F,F1, F1, F1, F1, DD, DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*W1     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,W2,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*W2     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,W3,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*W3     */{DD,F_F,F1,W4,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*W4     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,W_DONE,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*W_DONE */{DD, F_F,F1, F1, F1, F1, DD, DD, DD, DD, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, F1, DD, DD, DD, DD},
/*L1     */{DD,F_F,F1,F1,L2,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*L2     */{DD,F_F,F1,F1,L3,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*L3     */{DD,F_F,F1,F1,F1,L4,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*L4     */{L5,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*L5     */{DD,L6,L6,L6,L6,L6,DD,DD,DD,DD,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,DD,DD,DD,DD},
/*L6     */{DD,L6,L6,L6,L6,L6,DD,DD,DD,DD,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L6,L7,DD,DD,DD},
/*L7     */{DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,L8,DD,DD,DD},
/*L8     */{DD,DD,DD,DD,DD,DD,L_DONE,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD},
/*L_DONE */{DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD},
/*N1     */{DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,N1,DD,DD,DD},
/*SYM    */{DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD,DD},
/*R1     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,R2,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*R2     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,R3,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*R3     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,R4,F1,F1,F1,F1,DD,DD,DD,DD},
/*R4     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,R5,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*R5     */{DD,F_F,R_DONE,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*R_DONE */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*B1     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,B2,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*B2     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,B3,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*B3     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,B4,F1,F1,DD,DD,DD,DD},
/*B4     */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,B_DONE,F1,DD,DD,DD,DD},
/*B_DONE */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO1    */{DD,F_F,F1,F1,CO2,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO2    */{DD,F_F,CO3,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO3    */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,CO4,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO4    */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,CO5,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO5    */{DD,F_F,CO6,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO6    */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,CO7,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO7    */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,CO_DONE,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD},
/*CO_DON */{DD,F_F,F1,F1,F1,F1,DD,DD,DD,DD,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,F1,DD,DD,DD,DD}
};
#undef DD

static void run_lexer(const char *filename) {
    char *src = read_file(filename);
    if (!src) return;

    printf("\n  %-30s  %-14s  %s\n", "LEXEME", "TOKEN", "LINE");
    printf("  ----------------------------------------------------------------\n");

    int  state=S_START, lex_len=0, line_no=1;
    char lexeme[256];
    long src_len=(long)strlen(src);

    for (long i=0; i<=src_len; i++) {
        char ch = src[i];
        if (state==S_START && (ch==' '||ch=='\t'||ch=='\r'||ch=='\n'||ch=='\0')) {
            if (ch=='\n') line_no++;
            continue;
        }
        int col  = get_input(ch);
        int next = DFA[state][col];

        if (next==S_DEAD) {
            if (state!=S_START) {
                lexeme[lex_len]='\0';
                const char *tok = ACCEPT[state] ? ACCEPT[state] : "UNKNOWN";
                printf("  %-30s  %-14s  line %d\n", lexeme, tok, line_no);
                if (lex_count<MAX_TOKENS) {
                    strcpy(lex_tokens[lex_count].lexeme, lexeme);
                    lex_tokens[lex_count].token = tok;
                    lex_tokens[lex_count].line  = line_no;
                    lex_count++;
                }
                state=S_START; lex_len=0; i--;
            } else {
                if (ch!='\0')
                    printf("  %-30c  %-14s  line %d\n", ch, "UNKNOWN", line_no);
            }
        } else {
            if (ch!='\0') { lexeme[lex_len++]=ch; state=next; }
        }
    }
    printf("  ----------------------------------------------------------------\n");
    free(src);
}


/* ============================================================
   SECTION 2 - LL(1) PARSER
   Grammar (strictly per spec):
    1:  Program        -> Include Functions
    2:  Include        -> INCLUDE
    3:  Functions      -> FuncDef Functions
    4:  Functions      -> e
    5:  FuncDef        -> Type FUNCTION LPAREN FuncParams RPAREN LBRACKET StatementList RBRACKET
    6:  FuncParams     -> Type IDENTIFIER
    7:  FuncParams     -> e
    8:  StatementList  -> Statement StatementList
    9:  StatementList  -> e
   10:  Statement      -> Declaration
   11:  Statement      -> Assignment
   12:  Statement      -> LoopStatement
   13:  Statement      -> ReturnStatement
   14:  Statement      -> BreakStatement
   15:  Statement      -> PrintfStatement
   16:  Declaration    -> Type IDENTIFIER OPERATOR Expression DOTDOT
   17:  Assignment     -> IDENTIFIER OPERATOR Expression DOTDOT
   18:  LoopStatement  -> LOOP_LABEL WHILE LPAREN Type IDENTIFIER OPERATOR NUMBER DOTDOT RPAREN LBRACKET StatementList RBRACKET
   19:  ReturnStatement-> RETURN Expression DOTDOT
   20:  BreakStatement -> BREAK DOTDOT
   21:  PrintfStatement-> PRINTF LPAREN IDENTIFIER RPAREN DOTDOT
   22:  Expression     -> IDENTIFIER
   23:  Expression     -> NUMBER
   24:  Type           -> DEC
   25:  Type           -> INT
   ============================================================ */

#define MAXSTACK 200
#define MAXTOK   300
#define MAXSYM    64

/* Non-terminals */
enum {
    NT_PROGRAM=0, NT_INCLUDE, NT_FUNCTIONS, NT_FUNCDEF, NT_FUNCPARAMS,
    NT_STATEMENT_LIST, NT_STATEMENT,
    NT_DECLARATION, NT_ASSIGNMENT, NT_LOOP_STMT, NT_RETURN_STMT,
    NT_BREAK_STMT, NT_PRINTF_STMT,
    NT_EXPRESSION, NT_TYPE,
    NUM_NT
};

/* Terminals */
enum {
    T_INCLUDE=0, T_DEC, T_INT, T_IDENTIFIER, T_FUNCTION, T_LOOP_LABEL,
    T_WHILE, T_RETURN, T_BREAK, T_PRINTF, T_NUMBER, T_OPERATOR,
    T_DOTDOT, T_LBRACKET, T_RBRACKET, T_LPAREN, T_RPAREN, T_COLON,
    T_COMMA, T_EOF,
    NUM_TERMINALS
};

static const char *NT_NAMES[NUM_NT] = {
    "Program","Include","Functions","FuncDef","FuncParams",
    "StatementList","Statement",
    "Declaration","Assignment","LoopStatement","ReturnStatement",
    "BreakStatement","PrintfStatement",
    "Expression","Type"
};
static const char *T_NAMES[NUM_TERMINALS] = {
    "INCLUDE","DEC","INT","IDENTIFIER","FUNCTION","LOOP_LABEL",
    "WHILE","RETURN","BREAK","PRINTF","NUMBER","OPERATOR",
    "DOTDOT","LBRACKET","RBRACKET","LPAREN","RPAREN","COLON",
    "COMMA","EOF"
};

static const char *RHS[] = {
    /*0*/  "",
    /*1*/  "Include Functions",
    /*2*/  "INCLUDE",
    /*3*/  "FuncDef Functions",
    /*4*/  "",
    /*5*/  "Type FUNCTION LPAREN FuncParams RPAREN LBRACKET StatementList RBRACKET",
    /*6*/  "Type IDENTIFIER",
    /*7*/  "",
    /*8*/  "Statement StatementList",
    /*9*/  "",
    /*10*/ "Declaration",
    /*11*/ "Assignment",
    /*12*/ "LoopStatement",
    /*13*/ "ReturnStatement",
    /*14*/ "BreakStatement",
    /*15*/ "PrintfStatement",
    /*16*/ "Type IDENTIFIER OPERATOR Expression DOTDOT",
    /*17*/ "IDENTIFIER OPERATOR Expression DOTDOT",
    /*18*/ "LOOP_LABEL WHILE LPAREN Type IDENTIFIER OPERATOR NUMBER DOTDOT RPAREN LBRACKET StatementList RBRACKET",
    /*19*/ "RETURN Expression DOTDOT",
    /*20*/ "BREAK DOTDOT",
    /*21*/ "PRINTF LPAREN IDENTIFIER RPAREN DOTDOT",
    /*22*/ "IDENTIFIER",
    /*23*/ "NUMBER",
    /*24*/ "DEC",
    /*25*/ "INT"
};

/*
 *  LL(1) Parsing Table  [NT][Terminal] = production#  (0=error)
 *
 *               INC DEC INT IDN FUN LBL WHI RET BRK PRI NUM OPR DOT LBK RBK LPA RPA COL COM EOF
 */
static const int TABLE[NUM_NT][NUM_TERMINALS] = {
/*Program     */{ 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*Include     */{ 2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*Functions   */{ 0,  3,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4},
/*FuncDef     */{ 0,  5,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*FuncParams  */{ 0,  6,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  7,  0,  0,  0},
/*StatementLst*/{ 0,  8,  8,  8,  0,  8,  0,  8,  8,  8,  0,  0,  0,  0,  9,  0,  0,  0,  0,  9},
/*Statement   */{ 0, 10, 10, 11,  0, 12,  0, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*Declaration */{ 0, 16, 16,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*Assignment  */{ 0,  0,  0, 17,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*LoopStmt    */{ 0,  0,  0,  0,  0, 18,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*ReturnStmt  */{ 0,  0,  0,  0,  0,  0,  0, 19,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*BreakStmt   */{ 0,  0,  0,  0,  0,  0,  0,  0, 20,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*PrintfStmt  */{ 0,  0,  0,  0,  0,  0,  0,  0,  0, 21,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*Expression  */{ 0,  0,  0, 22,  0,  0,  0,  0,  0,  0, 23,  0,  0,  0,  0,  0,  0,  0,  0,  0},
/*Type        */{ 0, 24, 25,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}
};

/* ---- Stack ---- */
static char stk[MAXSTACK][MAXSYM];
static int  stk_top = -1;

static void stk_push(const char *s) {
    if (stk_top<MAXSTACK-1) { stk_top++; strcpy(stk[stk_top], s); }
}
static char *stk_pop(void) {
    return (stk_top>=0) ? stk[stk_top--] : NULL;
}

/* ---- Index helpers ---- */
static int find_nt(const char *x) {
    for (int i=0; i<NUM_NT; i++) if (strcmp(NT_NAMES[i],x)==0) return i;
    return -1;
}
static int find_t(const char *x) {
    for (int i=0; i<NUM_TERMINALS; i++) if (strcmp(T_NAMES[i],x)==0) return i;
    return -1;
}

/* ============================================================
   PARSER TOKENISER  (reads source directly; table-driven)
   ============================================================ */
typedef struct { char type[MAXSYM]; char lexeme[MAXSYM]; int line; } PToken;
static PToken ptoks[MAXTOK];
static int    n_ptoks = 0;

typedef enum {
    TT_INCLUDE=0, TT_COMMENT, TT_DEC, TT_INT, TT_IDENTIFIER,
    TT_FUNCTION, TT_LOOP_LABEL, TT_WHILE, TT_RETURN, TT_BREAK,
    TT_PRINTF, TT_NUMBER, TT_OPERATOR, TT_DOTDOT, TT_LBRACKET,
    TT_RBRACKET, TT_LPAREN, TT_RPAREN, TT_COLON, TT_COMMA,
    TT_EOF, TT_ERROR
} PTokenType;

static const char *TT_NAMES[] = {
    "INCLUDE","COMMENT","DEC","INT","IDENTIFIER",
    "FUNCTION","LOOP_LABEL","WHILE","RETURN","BREAK",
    "PRINTF","NUMBER","OPERATOR","DOTDOT","LBRACKET",
    "RBRACKET","LPAREN","RPAREN","COLON","COMMA",
    "EOF","UNKNOWN"
};

typedef struct { const char *word; PTokenType tt; } KwEntry;
static const KwEntry KW[] = {
    {"#include<stdio.h>", TT_INCLUDE },
    {"dec",               TT_DEC     },
    {"int",               TT_INT     },
    {"while",             TT_WHILE   },
    {"return",            TT_RETURN  },
    {"break",             TT_BREAK   },
    {"printfFn",          TT_PRINTF  },
    {"..",                TT_DOTDOT  },
    {"[",                 TT_LBRACKET},
    {"]",                 TT_RBRACKET},
    {"(",                 TT_LPAREN  },
    {")",                 TT_RPAREN  },
    {":",                 TT_COLON   },
    {",",                 TT_COMMA   },
    {"=",                 TT_OPERATOR},
    {"+",                 TT_OPERATOR},
    {"-",                 TT_OPERATOR},
    {"<",                 TT_OPERATOR},
    {"<=",                TT_OPERATOR}
};
static const int KW_LEN = (int)(sizeof KW / sizeof KW[0]);

static PTokenType classify(const char *lex) {
    for (int i=0; i<KW_LEN; i++)
        if (strcmp(lex, KW[i].word)==0) return KW[i].tt;
    if (isdigit((unsigned char)lex[0])) return TT_NUMBER;
    int n=(int)strlen(lex);
    if (n>=2 && lex[n-2]=='F' && lex[n-1]=='n') return TT_FUNCTION;
    if (strchr(lex, ':')) return TT_LOOP_LABEL;
    return TT_IDENTIFIER;
}

static void tokenise(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { printf("Error opening file!\n"); return; }
    char buf[20000];
    size_t got = fread(buf, 1, sizeof buf - 1, fp);
    buf[got] = '\0';
    fclose(fp);

    char *p = buf;
    int   line = 1;
    n_ptoks = 0;

    static const char SINGLES[] = "=+-<[]():,";

    while (*p && n_ptoks<MAXTOK) {
        while (*p==' '||*p=='\t'||*p=='\r') p++;
        if (*p=='\n') { line++; p++; continue; }
        if (*p=='\0') break;

        if (strncmp(p,"#include<stdio.h>",17)==0) {
            strcpy(ptoks[n_ptoks].lexeme,"#include<stdio.h>");
            ptoks[n_ptoks++].line=line; p+=17; continue;
        }
        if (p[0]=='/'&&p[1]=='/') {
            while (*p&&*p!='\n') p++; continue;
        }
        if (p[0]=='<'&&p[1]=='=') {
            strcpy(ptoks[n_ptoks].lexeme,"<=");
            ptoks[n_ptoks++].line=line; p+=2; continue;
        }
        if (p[0]=='.'&&p[1]=='.') {
            strcpy(ptoks[n_ptoks].lexeme,"..");
            ptoks[n_ptoks++].line=line; p+=2; continue;
        }
        int hit=0;
        for (int si=0; SINGLES[si]&&!hit; si++) {
            if (*p==SINGLES[si]) {
                ptoks[n_ptoks].lexeme[0]=*p;
                ptoks[n_ptoks].lexeme[1]='\0';
                ptoks[n_ptoks++].line=line;
                p++; hit=1;
            }
        }
        if (hit) continue;

        if (isdigit((unsigned char)*p)) {
            int k=0;
            while (isdigit((unsigned char)p[k])) k++;
            strncpy(ptoks[n_ptoks].lexeme,p,k);
            ptoks[n_ptoks].lexeme[k]='\0';
            ptoks[n_ptoks++].line=line; p+=k; continue;
        }
        if (isalpha((unsigned char)*p)||*p=='_') {
            int k=0;
            while (isalnum((unsigned char)p[k])||p[k]=='_') k++;
            strncpy(ptoks[n_ptoks].lexeme,p,k);
            ptoks[n_ptoks].lexeme[k]='\0';
            ptoks[n_ptoks++].line=line; p+=k; continue;
        }
        p++;
    }
    for (int i=0; i<n_ptoks; i++) {
        PTokenType tt = classify(ptoks[i].lexeme);
        strcpy(ptoks[i].type, TT_NAMES[tt]);
    }
    strcpy(ptoks[n_ptoks].type,   "EOF");
    strcpy(ptoks[n_ptoks].lexeme, "$");
    ptoks[n_ptoks].line=line;
    n_ptoks++;
}

static void run_ll1_parser(const char *filename) {
    tokenise(filename);

    printf("\n  %-5s  %-18s  %-20s  %s\n",
           "STEP","LOOKAHEAD","STACK TOP","ACTION / PRODUCTION");
    printf("  ---------------------------------------------------------------------------\n");

    stk_top=-1;
    stk_push("$");
    stk_push(NT_NAMES[NT_PROGRAM]);

    int ip=0, success=1, step=0;
    char verdict_detail[256]="";

    while (stk_top>=0) {
        char X[MAXSYM];
        strcpy(X, stk_pop());

        if (strcmp(X,"$")==0) break;

        const char *a = ptoks[ip].type;
        step++;

        char action[160];
        int tindex = find_t(X);

        if (tindex!=-1) {
            if (strcmp(X,a)==0) {
                sprintf(action,"match (%s)",X);
                printf("  %-5d  %-18s  %-20s  %s\n",step,a,X,action);
                ip++;
                if (ip>=n_ptoks) break;
            } else {
                sprintf(action,"ERROR");
                printf("  %-5d  %-18s  %-20s  %s\n",step,a,X,action);
                sprintf(verdict_detail,"Expected '%s', got '%s' at line %d",
                        X,a,ptoks[ip].line);
                success=0; break;
            }
            continue;
        }

        int ntindex=find_nt(X);
        int aindex =find_t(a);
        if (ntindex==-1||aindex==-1) {
            sprintf(action,"ERROR (unknown symbol)");
            printf("  %-5d  %-18s  %-20s  %s\n",step,a,X,action);
            sprintf(verdict_detail,"Unknown symbol '%s'",X);
            success=0; break;
        }

        int prod=TABLE[ntindex][aindex];
        if (prod==0) {
            sprintf(action,"ERROR (no rule)");
            printf("  %-5d  %-18s  %-20s  %s\n",step,a,X,action);
            sprintf(verdict_detail,"No rule for (%s , %s) at line %d",
                    X,a,ptoks[ip].line);
            success=0; break;
        }

        if (strlen(RHS[prod])==0) {
            sprintf(action,"%s -> epsilon",X);
            printf("  %-5d  %-18s  %-20s  %s\n",step,a,X,action);
            continue;
        }

        char tmp[300]; strcpy(tmp,RHS[prod]);
        const char *syms[30]; int ns=0;
        char *tok=strtok(tmp," ");
        while (tok) { syms[ns++]=tok; tok=strtok(NULL," "); }
        for (int i=ns-1; i>=0; i--) stk_push(syms[i]);

        char act80[81];
        snprintf(act80,sizeof act80,"%s -> %s",X,RHS[prod]);
        printf("  %-5d  %-18s  %-20s  %s\n",step,a,X,act80);
    }

    printf("  ---------------------------------------------------------------------------\n");

    /* SECTION 3: VERDICT */
    printf("\n  ----------------------------------------------------------------\n");
    printf("  PARSING VERDICT\n");
    printf("  ----------------------------------------------------------------\n");
    int accepted = success &&
                   (ip>=n_ptoks-1 || (ip==n_ptoks-1&&stk_top==-1));
    if (accepted)
        printf("  Verdict: ACCEPTED (Program conforms to the grammar)\n");
    else {
        printf("  Verdict: REJECTED (Program does NOT conform to the grammar)\n");
        if (verdict_detail[0])
            printf("  Reason : %s\n", verdict_detail);
    }
    printf("  ----------------------------------------------------------------\n\n");
}


/* ============================================================
   MAIN
   ============================================================ */
int main(int argc, char *argv[]) {
    const char *fn = (argc>=2) ? argv[1] : "test.txt";

    printf("==========================================================\n");
    printf("  SECTION 1 : LEXER TOKENISATION TABLE\n");
    printf("  Source file : %s\n", fn);
    printf("==========================================================\n");
    run_lexer(fn);

    printf("\n==========================================================\n");
    printf("  SECTION 2 : LL(1) PARSING TABLE\n");
    printf("==========================================================\n");
    run_ll1_parser(fn);

    return 0;
}