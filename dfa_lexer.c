#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// ============= TOKEN DEFINITIONS =============
typedef enum {
    TOKEN_INCLUDE, TOKEN_COMMENT, TOKEN_DEC, TOKEN_INT, TOKEN_IDENTIFIER,
    TOKEN_FUNCTION, TOKEN_LOOP_LABEL, TOKEN_WHILE, TOKEN_RETURN, TOKEN_BREAK,
    TOKEN_PRINTF, TOKEN_NUMBER, TOKEN_OPERATOR, TOKEN_DOTDOT, TOKEN_LBRACKET,
    TOKEN_RBRACKET, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_COLON, TOKEN_COMMA,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[100];
    int line;
    int column;
} Token;

// ============= DFA STATES =============
#define MAX_STATES 100
#define MAX_INPUTS 128

// DFA State definitions
enum {
    STATE_START = 0,
    STATE_INCLUDE, STATE_INCLUDE_DONE,
    STATE_COMMENT_SLASH, STATE_COMMENT, STATE_COMMENT_DONE,
    STATE_NUMBER,
    STATE_IDENTIFIER,
    STATE_VARIABLE_UNDER, STATE_VARIABLE_LETTERS, STATE_VARIABLE_DIGIT, STATE_VARIABLE_FINAL,
    STATE_FUNCTION, STATE_FUNCTION_F, STATE_FUNCTION_FN,
    STATE_KEYWORD_D, STATE_KEYWORD_DE, STATE_KEYWORD_DEC,
    STATE_KEYWORD_I, STATE_KEYWORD_IN, STATE_KEYWORD_INT,
    STATE_KEYWORD_W, STATE_KEYWORD_WH, STATE_KEYWORD_WHI, STATE_KEYWORD_WHIL, STATE_KEYWORD_WHILE,
    STATE_KEYWORD_R, STATE_KEYWORD_RE, STATE_KEYWORD_RET, STATE_KEYWORD_RETU, STATE_KEYWORD_RETUR, STATE_KEYWORD_RETURN,
    STATE_KEYWORD_B, STATE_KEYWORD_BR, STATE_KEYWORD_BRE, STATE_KEYWORD_BREA, STATE_KEYWORD_BREAK,
    STATE_KEYWORD_P, STATE_KEYWORD_PR, STATE_KEYWORD_PRI, STATE_KEYWORD_PRIN, STATE_KEYWORD_PRINT, STATE_KEYWORD_PRINTF,
    STATE_LOOP_L, STATE_LOOP_LO, STATE_LOOP_LOO, STATE_LOOP_LOOP, STATE_LOOP_UNDER,
    STATE_LOOP_LETTERS, STATE_LOOP_DIGIT1, STATE_LOOP_DIGIT2, STATE_LOOP_COLON,
    STATE_OPERATOR_EQ, STATE_OPERATOR_PLUS, STATE_OPERATOR_MINUS, STATE_OPERATOR_LT, STATE_OPERATOR_LE,
    STATE_DOT, STATE_DOTDOT,
    STATE_LBRACKET, STATE_RBRACKET, STATE_LPAREN, STATE_RPAREN, STATE_COLON, STATE_COMMA,
    STATE_DEAD = 99
};

// DFA Transition Table
int dfa[MAX_STATES][MAX_INPUTS];

// Accepting state info
int accepting[MAX_STATES];
TokenType accept_type[MAX_STATES];
int need_validation[MAX_STATES];

// Global lexer state
char *source;
int pos = 0;
int line = 1;
int col = 1;

// ============= VALIDATION FUNCTIONS =============
int validate_variable(char *str) {
    int len = strlen(str);
    if (len < 4 || str[0] != '_') return 0;
    int i = 1;
    while (i < len && isalpha(str[i])) i++;
    if (i == 1 || i >= len-1) return 0;
    if (!isdigit(str[i])) return 0;
    i++;
    if (i >= len || !isalpha(str[i])) return 0;
    i++;
    return (i == len);
}

int validate_loop_label(char *str) {
    int len = strlen(str);
    if (len < 8 || strncmp(str, "loop_", 5) != 0) return 0;
    int i = 5;
    while (i < len && isalpha(str[i])) i++;
    if (i == 5) return 0;
    if (i+1 >= len || !isdigit(str[i]) || !isdigit(str[i+1])) return 0;
    i += 2;
    return (i == len-1 && str[len-1] == ':');
}

// ============= DFA INITIALIZATION =============
void init_dfa() {
    // Initialize all to DEAD state
    for (int i = 0; i < MAX_STATES; i++)
        for (int j = 0; j < MAX_INPUTS; j++)
            dfa[i][j] = STATE_DEAD;
    
    // Clear accepting states
    for (int i = 0; i < MAX_STATES; i++) {
        accepting[i] = 0;
        need_validation[i] = 0;
    }
    
    // Setup DFA for #include<stdio.h>
    dfa[STATE_START]['#'] = STATE_INCLUDE;
    for (char c = 'a'; c <= 'z'; c++) {
        if (c == 'i') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'n') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'c') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'l') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'u') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'd') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'e') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 'h') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 's') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else if (c == 't') dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
        else dfa[STATE_INCLUDE][c] = STATE_INCLUDE;
    }
    dfa[STATE_INCLUDE]['<'] = STATE_INCLUDE;
    dfa[STATE_INCLUDE]['>'] = STATE_INCLUDE_DONE;
    dfa[STATE_INCLUDE]['.'] = STATE_INCLUDE;
    accepting[STATE_INCLUDE_DONE] = 1;
    accept_type[STATE_INCLUDE_DONE] = TOKEN_INCLUDE;
    
    // Setup DFA for comments
    dfa[STATE_START]['/'] = STATE_COMMENT_SLASH;
    dfa[STATE_COMMENT_SLASH]['/'] = STATE_COMMENT;
    for (int c = 'a'; c <= 'z'; c++) dfa[STATE_COMMENT][c] = STATE_COMMENT;
    dfa[STATE_COMMENT][' '] = STATE_COMMENT;
    accepting[STATE_COMMENT] = 1;
    accept_type[STATE_COMMENT] = TOKEN_COMMENT;
    
    // Setup DFA for numbers
    for (int d = '0'; d <= '9'; d++) {
        dfa[STATE_START][d] = STATE_NUMBER;
        dfa[STATE_NUMBER][d] = STATE_NUMBER;
    }
    accepting[STATE_NUMBER] = 1;
    accept_type[STATE_NUMBER] = TOKEN_NUMBER;
    
    // Setup DFA for underscore (variables start)
    dfa[STATE_START]['_'] = STATE_VARIABLE_UNDER;
    for (int c = 'a'; c <= 'z'; c++) {
        dfa[STATE_VARIABLE_UNDER][c] = STATE_VARIABLE_LETTERS;
        dfa[STATE_VARIABLE_LETTERS][c] = STATE_VARIABLE_LETTERS;
        dfa[STATE_VARIABLE_LETTERS][c] = STATE_VARIABLE_LETTERS;
    }
    for (int d = '0'; d <= '9'; d++) {
        dfa[STATE_VARIABLE_LETTERS][d] = STATE_VARIABLE_DIGIT;
    }
    for (int c = 'a'; c <= 'z'; c++) {
        dfa[STATE_VARIABLE_DIGIT][c] = STATE_VARIABLE_FINAL;
    }
    accepting[STATE_VARIABLE_FINAL] = 1;
    accept_type[STATE_VARIABLE_FINAL] = TOKEN_IDENTIFIER;
    need_validation[STATE_VARIABLE_FINAL] = 1;
    
    // Setup DFA for letters (keywords, functions, identifiers)
    for (int c = 'a'; c <= 'z'; c++) {
        dfa[STATE_START][c] = STATE_IDENTIFIER;
        dfa[STATE_IDENTIFIER][c] = STATE_IDENTIFIER;
        dfa[STATE_IDENTIFIER][c] = STATE_IDENTIFIER;
        dfa[STATE_IDENTIFIER][c] = STATE_IDENTIFIER;
    }
    
    // Keyword: dec
    dfa[STATE_START]['d'] = STATE_KEYWORD_D;
    dfa[STATE_KEYWORD_D]['e'] = STATE_KEYWORD_DE;
    dfa[STATE_KEYWORD_DE]['c'] = STATE_KEYWORD_DEC;
    accepting[STATE_KEYWORD_DEC] = 1;
    accept_type[STATE_KEYWORD_DEC] = TOKEN_DEC;
    
    // Keyword: int
    dfa[STATE_START]['i'] = STATE_KEYWORD_I;
    dfa[STATE_KEYWORD_I]['n'] = STATE_KEYWORD_IN;
    dfa[STATE_KEYWORD_IN]['t'] = STATE_KEYWORD_INT;
    accepting[STATE_KEYWORD_INT] = 1;
    accept_type[STATE_KEYWORD_INT] = TOKEN_INT;
    
    // Keyword: while
    dfa[STATE_START]['w'] = STATE_KEYWORD_W;
    dfa[STATE_KEYWORD_W]['h'] = STATE_KEYWORD_WH;
    dfa[STATE_KEYWORD_WH]['i'] = STATE_KEYWORD_WHI;
    dfa[STATE_KEYWORD_WHI]['l'] = STATE_KEYWORD_WHIL;
    dfa[STATE_KEYWORD_WHIL]['e'] = STATE_KEYWORD_WHILE;
    accepting[STATE_KEYWORD_WHILE] = 1;
    accept_type[STATE_KEYWORD_WHILE] = TOKEN_WHILE;
    
    // Keyword: return
    dfa[STATE_START]['r'] = STATE_KEYWORD_R;
    dfa[STATE_KEYWORD_R]['e'] = STATE_KEYWORD_RE;
    dfa[STATE_KEYWORD_RE]['t'] = STATE_KEYWORD_RET;
    dfa[STATE_KEYWORD_RET]['u'] = STATE_KEYWORD_RETU;
    dfa[STATE_KEYWORD_RETU]['r'] = STATE_KEYWORD_RETUR;
    dfa[STATE_KEYWORD_RETUR]['n'] = STATE_KEYWORD_RETURN;
    accepting[STATE_KEYWORD_RETURN] = 1;
    accept_type[STATE_KEYWORD_RETURN] = TOKEN_RETURN;
    
    // Keyword: break
    dfa[STATE_START]['b'] = STATE_KEYWORD_B;
    dfa[STATE_KEYWORD_B]['r'] = STATE_KEYWORD_BR;
    dfa[STATE_KEYWORD_BR]['e'] = STATE_KEYWORD_BRE;
    dfa[STATE_KEYWORD_BRE]['a'] = STATE_KEYWORD_BREA;
    dfa[STATE_KEYWORD_BREA]['k'] = STATE_KEYWORD_BREAK;
    accepting[STATE_KEYWORD_BREAK] = 1;
    accept_type[STATE_KEYWORD_BREAK] = TOKEN_BREAK;
    
    // Function detection (ends with Fn)
    for (int c = 'a'; c <= 'z'; c++) {
        dfa[STATE_IDENTIFIER][c] = STATE_FUNCTION;
        dfa[STATE_FUNCTION][c] = STATE_FUNCTION;
        dfa[STATE_FUNCTION]['F'] = STATE_FUNCTION_F;
        dfa[STATE_FUNCTION_F]['n'] = STATE_FUNCTION_FN;
    }
    accepting[STATE_FUNCTION_FN] = 1;
    accept_type[STATE_FUNCTION_FN] = TOKEN_FUNCTION;
    
    // Loop label detection
    dfa[STATE_START]['l'] = STATE_LOOP_L;
    dfa[STATE_LOOP_L]['o'] = STATE_LOOP_LO;
    dfa[STATE_LOOP_LO]['o'] = STATE_LOOP_LOO;
    dfa[STATE_LOOP_LOO]['p'] = STATE_LOOP_LOOP;
    dfa[STATE_LOOP_LOOP]['_'] = STATE_LOOP_UNDER;
    for (int c = 'a'; c <= 'z'; c++) {
        dfa[STATE_LOOP_UNDER][c] = STATE_LOOP_LETTERS;
        dfa[STATE_LOOP_LETTERS][c] = STATE_LOOP_LETTERS;
    }
    for (int d = '0'; d <= '9'; d++) {
        dfa[STATE_LOOP_LETTERS][d] = STATE_LOOP_DIGIT1;
        dfa[STATE_LOOP_DIGIT1][d] = STATE_LOOP_DIGIT2;
    }
    dfa[STATE_LOOP_DIGIT2][':'] = STATE_LOOP_COLON;
    accepting[STATE_LOOP_COLON] = 1;
    accept_type[STATE_LOOP_COLON] = TOKEN_LOOP_LABEL;
    need_validation[STATE_LOOP_COLON] = 1;
    
    // Operators
    dfa[STATE_START]['='] = STATE_OPERATOR_EQ;
    dfa[STATE_START]['+'] = STATE_OPERATOR_PLUS;
    dfa[STATE_START]['-'] = STATE_OPERATOR_MINUS;
    dfa[STATE_START]['<'] = STATE_OPERATOR_LT;
    dfa[STATE_OPERATOR_LT]['='] = STATE_OPERATOR_LE;
    accepting[STATE_OPERATOR_EQ] = accepting[STATE_OPERATOR_PLUS] = 
    accepting[STATE_OPERATOR_MINUS] = accepting[STATE_OPERATOR_LT] = 
    accepting[STATE_OPERATOR_LE] = 1;
    accept_type[STATE_OPERATOR_EQ] = accept_type[STATE_OPERATOR_PLUS] = 
    accept_type[STATE_OPERATOR_MINUS] = accept_type[STATE_OPERATOR_LT] = 
    accept_type[STATE_OPERATOR_LE] = TOKEN_OPERATOR;
    
    // Dot and dotdot
    dfa[STATE_START]['.'] = STATE_DOT;
    dfa[STATE_DOT]['.'] = STATE_DOTDOT;
    accepting[STATE_DOTDOT] = 1;
    accept_type[STATE_DOTDOT] = TOKEN_DOTDOT;
    
    // Single character tokens
    dfa[STATE_START]['['] = STATE_LBRACKET;
    dfa[STATE_START][']'] = STATE_RBRACKET;
    dfa[STATE_START]['('] = STATE_LPAREN;
    dfa[STATE_START][')'] = STATE_RPAREN;
    dfa[STATE_START][':'] = STATE_COLON;
    dfa[STATE_START][','] = STATE_COMMA;
    
    accepting[STATE_LBRACKET] = 1; accept_type[STATE_LBRACKET] = TOKEN_LBRACKET;
    accepting[STATE_RBRACKET] = 1; accept_type[STATE_RBRACKET] = TOKEN_RBRACKET;
    accepting[STATE_LPAREN] = 1; accept_type[STATE_LPAREN] = TOKEN_LPAREN;
    accepting[STATE_RPAREN] = 1; accept_type[STATE_RPAREN] = TOKEN_RPAREN;
    accepting[STATE_COLON] = 1; accept_type[STATE_COLON] = TOKEN_COLON;
    accepting[STATE_COMMA] = 1; accept_type[STATE_COMMA] = TOKEN_COMMA;
}

// ============= TOKEN GETTER USING DFA =============
void skip_whitespace() {
    while (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\n') {
        if (source[pos] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        pos++;
    }
}

Token get_next_token() {
    Token token;
    token.type = TOKEN_ERROR;
    token.lexeme[0] = '\0';
    token.line = line;
    token.column = col;
    
    skip_whitespace();
    
    if (source[pos] == '\0') {
        token.type = TOKEN_EOF;
        strcpy(token.lexeme, "EOF");
        return token;
    }
    
    // Special case for #include (not handled by DFA properly)
    if (strncmp(&source[pos], "#include<stdio.h>", 17) == 0) {
        token.type = TOKEN_INCLUDE;
        strncpy(token.lexeme, &source[pos], 17);
        token.lexeme[17] = '\0';
        pos += 17;
        col += 17;
        return token;
    }
    
    int state = STATE_START;
    int start_pos = pos;
    int start_line = line;
    int start_col = col;
    int last_accept = -1;
    int last_accept_pos = -1;
    
    while (1) {
        char c = source[pos];
        if (c == '\0' || c == ' ' || c == '\t' || c == '\n') break;
        
        int next = dfa[state][(unsigned char)c];
        if (next == STATE_DEAD) break;
        
        state = next;
        pos++;
        col++;
        
        if (accepting[state]) {
            last_accept = state;
            last_accept_pos = pos;
        }
    }
    
    if (last_accept != -1) {
        int len = last_accept_pos - start_pos;
        strncpy(token.lexeme, &source[start_pos], len);
        token.lexeme[len] = '\0';
        token.type = accept_type[last_accept];
        token.line = start_line;
        token.column = start_col;
        pos = last_accept_pos;
        col = start_col + len;
        
        if (need_validation[last_accept]) {
            if (token.type == TOKEN_IDENTIFIER && !validate_variable(token.lexeme))
                token.type = TOKEN_ERROR;
            if (token.type == TOKEN_LOOP_LABEL && !validate_loop_label(token.lexeme))
                token.type = TOKEN_ERROR;
        }
        
        return token;
    }
    
    // Error
    token.lexeme[0] = source[pos];
    token.lexeme[1] = '\0';
    token.type = TOKEN_ERROR;
    pos++;
    col++;
    return token;
}

// ============= MAIN =============
int main() {
    FILE *fp;
    char filename[100];
    char buffer[10000];
    
    printf("Enter source filename: ");
    scanf("%s", filename);
    
    fp = fopen(filename, "r");
    if (!fp) {
        printf("Error opening file!\n");
        return 1;
    }
    
    size_t bytes = fread(buffer, 1, 9999, fp);
    buffer[bytes] = '\0';
    source = buffer;
    fclose(fp);
    
    init_dfa();
    
    printf("\n+----------+------+------------------------+----------------------------------+\n");
    printf("| Line     | Col  | TOKEN TYPE             | LEXEME                           |\n");
    printf("+----------+------+------------------------+----------------------------------+\n");
    
    Token t;
    int errors = 0, total = 0;
    
    do {
        t = get_next_token();
        total++;
        
        char *type_str;
        switch(t.type) {
            case TOKEN_INCLUDE: type_str = "INCLUDE"; break;
            case TOKEN_COMMENT: type_str = "COMMENT"; break;
            case TOKEN_DEC: type_str = "DEC"; break;
            case TOKEN_INT: type_str = "INT"; break;
            case TOKEN_IDENTIFIER: type_str = "IDENTIFIER"; break;
            case TOKEN_FUNCTION: type_str = "FUNCTION"; break;
            case TOKEN_LOOP_LABEL: type_str = "LOOP_LABEL"; break;
            case TOKEN_WHILE: type_str = "WHILE"; break;
            case TOKEN_RETURN: type_str = "RETURN"; break;
            case TOKEN_BREAK: type_str = "BREAK"; break;
            case TOKEN_PRINTF: type_str = "PRINTF"; break;
            case TOKEN_NUMBER: type_str = "NUMBER"; break;
            case TOKEN_OPERATOR: type_str = "OPERATOR"; break;
            case TOKEN_DOTDOT: type_str = "DOTDOT"; break;
            case TOKEN_LBRACKET: type_str = "LBRACKET"; break;
            case TOKEN_RBRACKET: type_str = "RBRACKET"; break;
            case TOKEN_LPAREN: type_str = "LPAREN"; break;
            case TOKEN_RPAREN: type_str = "RPAREN"; break;
            case TOKEN_COLON: type_str = "COLON"; break;
            case TOKEN_COMMA: type_str = "COMMA"; break;
            case TOKEN_EOF: type_str = "EOF"; break;
            default: type_str = "ERROR"; errors++;
        }
        
        if (t.type != TOKEN_EOF) {
            printf("| %-8d | %-4d | %-22s | %-32s |\n", 
                   t.line, t.column, type_str, t.lexeme);
        }
    } while (t.type != TOKEN_EOF);
    
    printf("+----------+------+------------------------+----------------------------------+\n");
    printf("\nSUMMARY: Total=%d, Errors=%d, %s\n", total-1, errors, errors?"FAILED":"PASSED");
    
    return 0;
}