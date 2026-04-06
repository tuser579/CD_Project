#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAXSTACK 200
#define MAXTOK 200
#define MAXSYM 50

// ============= TOKEN TYPES (from lexer) =============
typedef enum {
    TOKEN_INCLUDE, TOKEN_COMMENT, TOKEN_DEC, TOKEN_INT, TOKEN_IDENTIFIER,
    TOKEN_FUNCTION, TOKEN_LOOP_LABEL, TOKEN_WHILE, TOKEN_RETURN, TOKEN_BREAK,
    TOKEN_PRINTF, TOKEN_NUMBER, TOKEN_OPERATOR, TOKEN_DOTDOT, TOKEN_LBRACKET,
    TOKEN_RBRACKET, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_COLON, TOKEN_COMMA,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;
 
// ============= GRAMMAR NON-TERMINALS =============
enum {
    NT_PROGRAM, NT_INCLUDE, NT_CUSTOM_FUNCTIONS, NT_CUSTOM_FUNCTION,
    NT_MAIN_FUNCTION, NT_PARAMETER, NT_STATEMENT_LIST, NT_STATEMENT,
    NT_DECLARATION, NT_ASSIGNMENT, NT_WHILE_STATEMENT, NT_RETURN_STATEMENT,
    NT_BREAK_STATEMENT, NT_PRINTF_STATEMENT, NT_EXPRESSION, NT_TYPE,
    NUM_NT
};

// ============= TERMINALS =============
enum {
    T_INCLUDE, T_DEC, T_INT, T_IDENTIFIER, T_FUNCTION, T_LOOP_LABEL,
    T_WHILE, T_RETURN, T_BREAK, T_PRINTF, T_NUMBER, T_OPERATOR,
    T_DOTDOT, T_LBRACKET, T_RBRACKET, T_LPAREN, T_RPAREN, T_COLON,
    T_COMMA, T_EOF,
    NUM_TERMINALS
};

// Names for display
char *NT_NAMES[NUM_NT] = {
    "Program", "Include", "CustomFunctions", "CustomFunction",
    "MainFunction", "Parameter", "StatementList", "Statement",
    "Declaration", "Assignment", "WhileStatement", "ReturnStatement",
    "BreakStatement", "PrintfStatement", "Expression", "Type"
};

char *T_NAMES[NUM_TERMINALS] = {
    "INCLUDE", "DEC", "INT", "IDENTIFIER", "FUNCTION", "LOOP_LABEL",
    "WHILE", "RETURN", "BREAK", "PRINTF", "NUMBER", "OPERATOR",
    "DOTDOT", "LBRACKET", "RBRACKET", "LPAREN", "RPAREN", "COLON",
    "COMMA", "EOF"
};

// Productions RHS
char *RHS[] = {
    "",  // 0
    "Include CustomFunctions MainFunction",  // 1
    "INCLUDE",                                // 2
    "CustomFunction CustomFunctions",         // 3
    "",                                       // 4
    "Type FUNCTION LPAREN Parameter RPAREN LBRACKET StatementList RBRACKET", // 5
    "Type FUNCTION LPAREN RPAREN LBRACKET StatementList RBRACKET", // 6
    "Type IDENTIFIER",                        // 7
    "Statement StatementList",                // 8
    "",                                       // 9
    "Declaration",                            // 10
    "Assignment",                             // 11
    "WhileStatement",                         // 12
    "ReturnStatement",                        // 13
    "BreakStatement",                         // 14
    "PrintfStatement",                        // 15
    "Type IDENTIFIER OPERATOR Expression DOTDOT", // 16
    "IDENTIFIER OPERATOR Expression DOTDOT",      // 17
    "WHILE LPAREN Type IDENTIFIER OPERATOR NUMBER DOTDOT RPAREN LBRACKET StatementList RBRACKET", // 18
    "RETURN Expression DOTDOT",               // 19
    "BREAK DOTDOT",                           // 20
    "PRINTF LPAREN IDENTIFIER RPAREN DOTDOT", // 21
    "IDENTIFIER",                             // 22
    "NUMBER",                                 // 23
    "DEC",                                    // 24
    "INT"                                     // 25
};

// LL(1) Parsing Table
int TABLE[NUM_NT][NUM_TERMINALS];

// ============= STACK =============
char stack[MAXSTACK][MAXSYM];
int top = -1;

void push(char *s) {
    if (top < MAXSTACK-1) strcpy(stack[++top], s);
}

char* pop() {
    return (top >= 0) ? stack[top--] : NULL;
}

void print_stack() {
    printf("[");
    for (int i = top; i >= 0; i--) {
        printf("%s", stack[i]);
        if (i > 0) printf(", ");
    }
    printf("]");
}

// ============= HELPERS =============
int find_nt(char *x) {
    for (int i = 0; i < NUM_NT; i++)
        if (strcmp(NT_NAMES[i], x) == 0) return i;
    return -1;
}

int find_t(char *x) {
    for (int i = 0; i < NUM_TERMINALS; i++)
        if (strcmp(T_NAMES[i], x) == 0) return i;
    return -1;
}

// ============= INITIALIZE PARSING TABLE =============
void init_table() {
    for (int i = 0; i < NUM_NT; i++)
        for (int j = 0; j < NUM_TERMINALS; j++)
            TABLE[i][j] = 0;
    
    // Program → Include CustomFunctions MainFunction
    TABLE[NT_PROGRAM][T_INCLUDE] = 1;
    
    // Include → INCLUDE
    TABLE[NT_INCLUDE][T_INCLUDE] = 2;
    
    // CustomFunctions → CustomFunction CustomFunctions
    TABLE[NT_CUSTOM_FUNCTIONS][T_DEC] = 3;
    TABLE[NT_CUSTOM_FUNCTIONS][T_INT] = 3;
    // CustomFunctions → ε
    TABLE[NT_CUSTOM_FUNCTIONS][T_EOF] = 4;
    
    // CustomFunction → Type FUNCTION LPAREN Parameter RPAREN LBRACKET StatementList RBRACKET
    TABLE[NT_CUSTOM_FUNCTION][T_DEC] = 5;
    TABLE[NT_CUSTOM_FUNCTION][T_INT] = 5;
    
    // MainFunction → Type FUNCTION LPAREN RPAREN LBRACKET StatementList RBRACKET
    TABLE[NT_MAIN_FUNCTION][T_DEC] = 6;
    TABLE[NT_MAIN_FUNCTION][T_INT] = 6;
    
    // Parameter → Type IDENTIFIER
    TABLE[NT_PARAMETER][T_DEC] = 7;
    TABLE[NT_PARAMETER][T_INT] = 7;
    
    // StatementList → Statement StatementList
    TABLE[NT_STATEMENT_LIST][T_DEC] = 8;
    TABLE[NT_STATEMENT_LIST][T_INT] = 8;
    TABLE[NT_STATEMENT_LIST][T_IDENTIFIER] = 8;
    TABLE[NT_STATEMENT_LIST][T_WHILE] = 8;
    TABLE[NT_STATEMENT_LIST][T_RETURN] = 8;
    TABLE[NT_STATEMENT_LIST][T_BREAK] = 8;
    TABLE[NT_STATEMENT_LIST][T_PRINTF] = 8;
    // StatementList → ε
    TABLE[NT_STATEMENT_LIST][T_RBRACKET] = 9;
    TABLE[NT_STATEMENT_LIST][T_EOF] = 9;
    
    // Statement → Declaration
    TABLE[NT_STATEMENT][T_DEC] = 10;
    TABLE[NT_STATEMENT][T_INT] = 10;
    // Statement → Assignment
    TABLE[NT_STATEMENT][T_IDENTIFIER] = 11;
    // Statement → WhileStatement
    TABLE[NT_STATEMENT][T_WHILE] = 12;
    // Statement → ReturnStatement
    TABLE[NT_STATEMENT][T_RETURN] = 13;
    // Statement → BreakStatement
    TABLE[NT_STATEMENT][T_BREAK] = 14;
    // Statement → PrintfStatement
    TABLE[NT_STATEMENT][T_PRINTF] = 15;
    
    // Declaration → Type IDENTIFIER OPERATOR Expression DOTDOT
    TABLE[NT_DECLARATION][T_DEC] = 16;
    TABLE[NT_DECLARATION][T_INT] = 16;
    
    // Assignment → IDENTIFIER OPERATOR Expression DOTDOT
    TABLE[NT_ASSIGNMENT][T_IDENTIFIER] = 17;
    
    // WhileStatement → WHILE LPAREN Type IDENTIFIER OPERATOR NUMBER DOTDOT RPAREN LBRACKET StatementList RBRACKET
    TABLE[NT_WHILE_STATEMENT][T_WHILE] = 18;
    
    // ReturnStatement → RETURN Expression DOTDOT
    TABLE[NT_RETURN_STATEMENT][T_RETURN] = 19;
    
    // BreakStatement → BREAK DOTDOT
    TABLE[NT_BREAK_STATEMENT][T_BREAK] = 20;
    
    // PrintfStatement → PRINTF LPAREN IDENTIFIER RPAREN DOTDOT
    TABLE[NT_PRINTF_STATEMENT][T_PRINTF] = 21;
    
    // Expression → IDENTIFIER
    TABLE[NT_EXPRESSION][T_IDENTIFIER] = 22;
    // Expression → NUMBER
    TABLE[NT_EXPRESSION][T_NUMBER] = 23;
    
    // Type → DEC
    TABLE[NT_TYPE][T_DEC] = 24;
    // Type → INT
    TABLE[NT_TYPE][T_INT] = 25;
}

// ============= SIMPLE LEXER FOR PARSER =============
typedef struct {
    char type[MAXSYM];
    char lexeme[MAXSYM];
    int line;
} ParserToken;

ParserToken tokens[MAXTOK];
int num_tokens = 0;

TokenType get_token_type(char *lexeme) {
    if (strcmp(lexeme, "#include<stdio.h>") == 0) return TOKEN_INCLUDE;
    if (strcmp(lexeme, "dec") == 0) return TOKEN_DEC;
    if (strcmp(lexeme, "int") == 0) return TOKEN_INT;
    if (strcmp(lexeme, "while") == 0) return TOKEN_WHILE;
    if (strcmp(lexeme, "return") == 0) return TOKEN_RETURN;
    if (strcmp(lexeme, "break") == 0) return TOKEN_BREAK;
    if (strcmp(lexeme, "printfFn") == 0) return TOKEN_PRINTF;
    if (strcmp(lexeme, "..") == 0) return TOKEN_DOTDOT;
    if (strcmp(lexeme, "[") == 0) return TOKEN_LBRACKET;
    if (strcmp(lexeme, "]") == 0) return TOKEN_RBRACKET;
    if (strcmp(lexeme, "(") == 0) return TOKEN_LPAREN;
    if (strcmp(lexeme, ")") == 0) return TOKEN_RPAREN;
    if (strcmp(lexeme, ":") == 0) return TOKEN_COLON;
    if (strcmp(lexeme, ",") == 0) return TOKEN_COMMA;
    if (strcmp(lexeme, "=") == 0 || strcmp(lexeme, "+") == 0 || 
        strcmp(lexeme, "-") == 0 || strcmp(lexeme, "<") == 0 || strcmp(lexeme, "<=") == 0)
        return TOKEN_OPERATOR;
    if (isdigit(lexeme[0])) return TOKEN_NUMBER;
    
    // Check for function (ends with Fn)
    int len = strlen(lexeme);
    if (len >= 2 && lexeme[len-2] == 'F' && lexeme[len-1] == 'n')
        return TOKEN_FUNCTION;
    
    // Check for loop label (contains :)
    if (strchr(lexeme, ':') != NULL)
        return TOKEN_LOOP_LABEL;
    
    // Check for variable (starts with _)
    if (lexeme[0] == '_')
        return TOKEN_IDENTIFIER;
    
    return TOKEN_IDENTIFIER;
}

char* token_to_string(TokenType type) {
    switch(type) {
        case TOKEN_INCLUDE: return "INCLUDE";
        case TOKEN_DEC: return "DEC";
        case TOKEN_INT: return "INT";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_FUNCTION: return "FUNCTION";
        case TOKEN_LOOP_LABEL: return "LOOP_LABEL";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_PRINTF: return "PRINTF";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_OPERATOR: return "OPERATOR";
        case TOKEN_DOTDOT: return "DOTDOT";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_COLON: return "COLON";
        case TOKEN_COMMA: return "COMMA";
        default: return "UNKNOWN";
    }
}

void tokenize_file(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error opening file!\n");
        return;
    }
    
    char buffer[10000];
    fread(buffer, 1, 9999, fp);
    fclose(fp);
    
    // Simple tokenization
    char *p = buffer;
    num_tokens = 0;
    
    while (*p && num_tokens < MAXTOK) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '\0') break;
        
        // Handle #include<stdio.h>
        if (strncmp(p, "#include<stdio.h>", 17) == 0) {
            strcpy(tokens[num_tokens].lexeme, "#include<stdio.h>");
            tokens[num_tokens].type[0] = '\0';
            num_tokens++;
            p += 17;
            continue;
        }
        
        // Handle comments
        if (*p == '/' && *(p+1) == '/') {
            while (*p != '\n' && *p != '\0') p++;
            continue;
        }
        
        // Handle operators and symbols
        if (*p == '=' || *p == '+' || *p == '-' || *p == '<') {
            if (*p == '<' && *(p+1) == '=') {
                strcpy(tokens[num_tokens].lexeme, "<=");
                p += 2;
            } else {
                tokens[num_tokens].lexeme[0] = *p;
                tokens[num_tokens].lexeme[1] = '\0';
                p++;
            }
            tokens[num_tokens].type[0] = '\0';
            num_tokens++;
            continue;
        }
        
        if (*p == '.' && *(p+1) == '.') {
            strcpy(tokens[num_tokens].lexeme, "..");
            p += 2;
            tokens[num_tokens].type[0] = '\0';
            num_tokens++;
            continue;
        }
        
        // Handle single character tokens
        if (strchr("[](){}:,.", *p)) {
            tokens[num_tokens].lexeme[0] = *p;
            tokens[num_tokens].lexeme[1] = '\0';
            p++;
            tokens[num_tokens].type[0] = '\0';
            num_tokens++;
            continue;
        }
        
        // Handle numbers
        if (isdigit(*p)) {
            int start = 0;
            while (isdigit(p[start])) start++;
            strncpy(tokens[num_tokens].lexeme, p, start);
            tokens[num_tokens].lexeme[start] = '\0';
            p += start;
            tokens[num_tokens].type[0] = '\0';
            num_tokens++;
            continue;
        }
        
        // Handle identifiers, keywords
        if (isalpha(*p) || *p == '_') {
            int start = 0;
            while (isalnum(p[start]) || p[start] == '_') start++;
            strncpy(tokens[num_tokens].lexeme, p, start);
            tokens[num_tokens].lexeme[start] = '\0';
            p += start;
            tokens[num_tokens].type[0] = '\0';
            num_tokens++;
            continue;
        }
        
        p++;
    }
    
    // Convert to parser terminals
    for (int i = 0; i < num_tokens; i++) {
        TokenType tt = get_token_type(tokens[i].lexeme);
        strcpy(tokens[i].type, token_to_string(tt));
    }
    
    // Add EOF
    strcpy(tokens[num_tokens].type, "EOF");
    strcpy(tokens[num_tokens].lexeme, "$");
    num_tokens++;
}

// ============= MAIN PARSER =============
int main() {
    char filename[100];
    
    printf("Enter source filename: ");
    scanf("%s", filename);
    
    // Tokenize the file
    tokenize_file(filename);
    
    printf("\n========== TOKENS FROM FILE ==========\n");
    for (int i = 0; i < num_tokens; i++) {
        printf("%d: %-15s %s\n", i+1, tokens[i].type, tokens[i].lexeme);
    }
    
    // Initialize parsing table
    init_table();
    
    printf("\n========== LL(1) PARSING ==========\n");
    printf("\n%-35s %-15s %-15s %-35s\n", "Stack", "Lookahead", "Top", "Production Applied");
    printf("----------------------------------------------------------------------------------------------------\n");
    
    // Initialize stack
    push("$");
    push(NT_NAMES[NT_PROGRAM]);
    
    int ip = 0;
    int step = 1;
    
    while (top >= 0) {
        char X[MAXSYM];
        strcpy(X, pop());
        
        char *a = tokens[ip].type;
        
        printf("%-35s %-15s %-15s ", "", a, X);
        
        int tindex = find_t(X);
        
        if (tindex != -1) { // X is a terminal
            if (strcmp(X, a) == 0) {
                printf("%-35s\n", "match");
                ip++;
                if (ip >= num_tokens) break;
            } else {
                printf("REJECTED - Expected %s, got %s\n", X, a);
                break;
            }
            print_stack();
            printf("\n");
            step++;
            continue;
        }
        
        int ntindex = find_nt(X);
        int aindex = find_t(a);
        
        if (ntindex == -1 || aindex == -1) {
            printf("REJECTED - Unknown symbol\n");
            break;
        }
        
        int prod = TABLE[ntindex][aindex];
        
        if (prod == 0) {
            printf("REJECTED - No rule for (%s, %s)\n", X, a);
            break;
        }
        
        if (strlen(RHS[prod]) == 0) {
            printf("%-35s\n", "ε");
        } else {
            printf("%-35s\n", RHS[prod]);
        }
        
        if (strlen(RHS[prod]) > 0) {
            char temp[200];
            strcpy(temp, RHS[prod]);
            
            char *symbols[20];
            int num_symbols = 0;
            char *tok = strtok(temp, " ");
            while (tok) {
                symbols[num_symbols++] = tok;
                tok = strtok(NULL, " ");
            }
            
            for (int i = num_symbols - 1; i >= 0; i--) {
                push(symbols[i]);
            }
        }
        
        print_stack();
        printf("\n");
        step++;
    }
    
    printf("\n========== PARSING VERDICT ==========\n");
    if (ip >= num_tokens - 1 || (ip == num_tokens - 1 && top == -1)) {
        printf("\n✅✅✅ ACCEPTED: Program follows the grammar ✅✅✅\n");
    } else {
        printf("\n❌❌❌ REJECTED: Program does NOT follow the grammar ❌❌❌\n");
    }
    
    return 0;
}