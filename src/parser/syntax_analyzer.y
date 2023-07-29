%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "syntax_tree.h"

// external functions from lex
extern int yylex();
extern int yyparse();
extern int yyrestart();
extern FILE * yyin;

// external variables from lexical_analyzer module
extern int lines;
extern char * yytext;
extern int pos_end;
extern int pos_start;

// Global syntax tree
syntax_tree *gt;

// Error reporting
void yyerror(const char *s);

// Helper functions written for you with love
syntax_tree_node *node(const char *node_name, int children_num, ...);
%}

/* TODO: Complete this definition.
   Hint: See pass_node(), node(), and syntax_tree.h.
         Use forward declaring. */
%union {
    struct _syntax_tree_node *node;
}

/* TODO: Your tokens here. */
%token <node> ERROR
%token <node> ADD
%token <node> ELSE
%token <node> IF
%token <node> INT
%token <node> RETURN
%token <node> VOID
%token <node> WHILE
%token <node> FLOAT
%token <node> SUB
%token <node> MUL
%token <node> DIV
%token <node> LT
%token <node> LTE
%token <node> GT
%token <node> GTE
%token <node> AEQ
%token <node> NEQ
%token <node> EQ
%token <node> SEMI
%token <node> COMMA
%token <node> LP
%token <node> RP
%token <node> LB
%token <node> RB
%token <node> LCB
%token <node> RCB
%token <node> ID
%token <node> INTEGER
%token <node> FLOATPOINT

%type <node> program
%type <node> type-specifier
%type <node> relop
%type <node> addop
%type <node> mulop
%type <node> declaration-list
%type <node> declaration
%type <node> var-declaration
%type <node> fun-declaration
%type <node> local-declarations
%type <node> compound-stmt
%type <node> statement-list
%type <node> statement
%type <node> expression-stmt
%type <node> iteration-stmt
%type <node> selection-stmt
%type <node> return-stmt
%type <node> expression
%type <node> simple-expression
%type <node> var
%type <node> additive-expression
%type <node> term
%type <node> factor
%type <node> integer
%type <node> float
%type <node> call
%type <node> params
%type <node> param-list
%type <node> param
%type <node> args
%type <node> arg-list
%type <node> empty

%start program

%%
/* TODO: Your rules here. */

/* Example:
program: declaration-list {$$ = node( "program", 1, $1); gt->root = $$;}
       ;
*/

program : declaration-list {$$ = node( "program", 1, $1); gt->root = $$; }
        ;
declaration-list : declaration-list declaration {$$ = node( "declaration-list", 2, $1, $2); } 
        | declaration {$$ = node( "declaration-list", 1, $1); }
        ;
declaration : var-declaration {$$ = node( "declaration", 1, $1); } 
        | fun-declaration {$$ = node( "declaration", 1, $1); }
        ;
var-declaration: type-specifier ID SEMI {$$ = node( "var-declaration", 3, $1,$2,$3); }
        | type-specifier ID LB INTEGER RB SEMI {$$ = node( "var-declaration", 6, $1, $2, $3, $4, $5, $6); }
        ;
type-specifier: INT {$$ = node( "type-specifier", 1, $1); } 
        | FLOAT {$$ = node( "type-specifier", 1, $1); } 
        | VOID {$$ = node( "type-specifier", 1, $1); }
        ;
fun-declaration : type-specifier ID LP params RP compound-stmt {$$ = node( "fun-declaration", 6, $1, $2, $3, $4, $5, $6); }
        ;
params : param-list {$$ = node( "params", 1, $1); }
        | VOID {$$ = node( "params", 1, $1); }
        ;
param-list : param-list COMMA param {$$ = node( "param-list", 3, $1, $2, $3);}
        | param {$$ = node( "param-list", 1, $1); }
        ;
param : type-specifier ID {$$ = node( "param", 2, $1, $2); }
        | type-specifier ID LB RB {$$ = node( "param", 4, $1, $2, $3, $4); }
        ;
compound-stmt : LCB local-declarations statement-list RCB {$$ = node( "compound-stmt", 4, $1, $2, $3, $4); }
        ;
local-declarations : local-declarations var-declaration {$$ = node( "local-declarations", 2, $1, $2); }
        | empty {$$ = node( "local-declarations", 0); }
        ;
statement-list: statement-list statement {$$ = node( "statement-list", 2, $1, $2); }
        | empty {$$ = node( "statement-list", 0); }
        ;
statement: expression-stmt {$$ = node( "statement", 1, $1); }
        | compound-stmt {$$ = node( "statement", 1, $1); }
        | selection-stmt {$$ = node( "statement", 1, $1); }
        | iteration-stmt {$$ = node( "statement", 1, $1); }
        | return-stmt {$$ = node( "statement", 1, $1); }
        ; 
expression-stmt: expression SEMI {$$ = node( "expression-stmt", 2, $1, $2); }
        | SEMI {$$ = node( "expression-stmt", 1, $1); }
        ;
selection-stmt: IF LP expression RP statement {$$ = node( "selection-stmt", 5, $1, $2, $3, $4, $5); }
        | IF LP expression RP statement ELSE statement {$$ = node( "selection-stmt", 7, $1, $2, $3, $4, $5, $6, $7); }
        ;
iteration-stmt: WHILE LP expression RP statement {$$ = node( "iteration-stmt", 5, $1, $2 ,$3 ,$4 ,$5); }
        ;
return-stmt: RETURN SEMI {$$ = node( "return-stmt", 2,$1,$2);  }
        | RETURN expression SEMI {$$ = node( "return-stmt", 3, $1, $2, $3); }
        ;
expression: var EQ expression {$$ = node( "expression", 3, $1, $2, $3); }
        | simple-expression {$$ = node( "expression", 1, $1); }
        ;
var: ID {$$ = node( "var", 1, $1); }
        | ID LB expression RB {$$ = node( "var", 4, $1, $2, $3, $4); }
        ;
simple-expression: additive-expression relop additive-expression {$$ = node( "simple-expression", 3, $1, $2, $3); }
        | additive-expression {$$ = node( "simple-expression", 1, $1); }
        ;
relop: LTE {$$ = node( "relop", 1, $1); }
        | LT {$$ = node( "relop", 1, $1); }
        | GT {$$ = node( "relop", 1, $1); }
        | GTE {$$ = node( "relop", 1, $1); }
        | AEQ {$$ = node( "relop", 1, $1); }
        | NEQ {$$ = node( "relop", 1, $1); }
        ;
additive-expression: additive-expression addop term {$$ = node( "additive-expression", 3, $1, $2, $3); }
        | term {$$ = node( "additive-expression", 1, $1); }
        ;
addop: ADD {$$ = node( "addop", 1, $1); }
        | SUB {$$ = node( "addop", 1, $1); }
        ;
term: term mulop factor {$$ = node( "term", 3, $1, $2, $3); }
        | factor {$$ = node( "term", 1, $1); }
        ;
mulop: MUL {$$ = node( "mulop", 1, $1); }
        | DIV {$$ = node( "mulop", 1, $1); }
        ;
factor: LP expression RP {$$ = node( "factor", 3, $1, $2, $3); }
        | var {$$ = node( "factor", 1, $1); }
        | call {$$ = node( "factor", 1, $1); }
        | integer {$$ = node( "factor", 1, $1); }
        | float {$$ = node( "factor", 1, $1); }
        ;
integer: INTEGER {$$ = node( "integer", 1, $1); }
        ;
float: FLOATPOINT {$$ = node( "float", 1, $1); }
        ;
call: ID LP args RP {$$ = node( "call", 4, $1, $2, $3, $4); }
        ;
args: arg-list {$$ = node( "args", 1, $1); }
        | empty {$$ = node( "args", 0); }
        ;
arg-list: arg-list COMMA expression {$$ = node( "arg-list", 3,$1,$2,$3); }
        | expression {$$ = node( "arg-list", 1,$1); }
        ;
empty: {} ;
%%

/// The error reporting function.
void yyerror(const char * s)
{
    // TO STUDENTS: This is just an example.
    // You can customize it as you like.
    fprintf(stderr, "error at line %d column %d: %s\n", lines, pos_start, s);
}

/// Parse input from file `input_path`, and prints the parsing results
/// to stdout.  If input_path is NULL, read from stdin.
///
/// This function initializes essential states before running yyparse().
syntax_tree *parse(const char *input_path)
{
    if (input_path != NULL) {
        if (!(yyin = fopen(input_path, "r"))) {
            fprintf(stderr, "[ERR] Open input file %s failed.\n", input_path);
            exit(1);
        }
    } else {
        yyin = stdin;
    }

    lines = pos_start = pos_end = 1;
    gt = new_syntax_tree();
    yyrestart(yyin);
    yyparse();
    return gt;
}

/// A helper function to quickly construct a tree node.
///
/// e.g. $$ = node("program", 1, $1);
syntax_tree_node *node(const char *name, int children_num, ...)
{
    syntax_tree_node *p = new_syntax_tree_node(name);
    syntax_tree_node *child;
    if (children_num == 0) {
        child = new_syntax_tree_node("epsilon");
        syntax_tree_add_child(p, child);
    } else {
        va_list ap;
        va_start(ap, children_num);
        for (int i = 0; i < children_num; ++i) {
            child = va_arg(ap, syntax_tree_node *);
            syntax_tree_add_child(p, child);
        }
        va_end(ap);
    }
    return p;
}
