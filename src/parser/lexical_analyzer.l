%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"

int lines;
int pos_start;
int pos_end;

void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

%}


%%
 /* to do for students */
 /* two cases for you, pass_node will send flex's token to bison */
else {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return ELSE;}
if {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return IF;}
int {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return INT;}
return {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return RETURN;}
void {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return VOID;}
while {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return WHILE;}
float {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return FLOAT;}
\+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;}
\- {pos_start = pos_end; pos_end += 1; pass_node(yytext); return SUB;}
\* {pos_start = pos_end; pos_end += 1; pass_node(yytext); return MUL;}
\/ {pos_start = pos_end; pos_end += 1; pass_node(yytext); return DIV;}
\n {pos_start = 0; pos_end = 1; lines++;}
\< {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LT;}
\<\= {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LTE;}
\> {pos_start = pos_end; pos_end += 1; pass_node(yytext); return GT;}
\>\= {pos_start = pos_end; pos_end += 1; pass_node(yytext); return GTE;}
\=\= {pos_start = pos_end; pos_end += 1; pass_node(yytext); return AEQ;}
\!\= {pos_start = pos_end; pos_end += 1; pass_node(yytext); return NEQ;}
\= {pos_start = pos_end; pos_end += 1; pass_node(yytext); return EQ;}
\; {pos_start = pos_end; pos_end += 1; pass_node(yytext); return SEMI;}
\, {pos_start = pos_end; pos_end += 1; pass_node(yytext); return COMMA;}
\( {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LP;}
\) {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RP;}
\[ {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LB;}
\] {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RB;}
\{ {pos_start = pos_end; pos_end += 1; pass_node(yytext); return LCB;}
\} {pos_start = pos_end; pos_end += 1; pass_node(yytext); return RCB;}
[0-9]+\.[0-9]*|[0-9]*\.[0-9]+ {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return FLOATPOINT;}
[a-zA-Z]+ {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return ID;}
0|[1-9][0-9]* {pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return INTEGER;}
\/\*([^*]|(\*+[^*/]))*\*+\/ {for(int i = 0; i < strlen(yytext); i++) {if(yytext[i] == '\n') {lines++;}} pos_start = 0; pos_end = 1;}
" " {pos_end += strlen(yytext); pos_start = pos_end;}
\t {pos_end += 4; pos_start = pos_end;}
. { pos_start = pos_end; pos_end++; return ERROR; }

 /****请在此补全所有flex的模式与动作  end******/

%%
