
%option yylineno
%option noyywrap
%option warn
%option nodefault
%option never-interactive

%top {
enum {
    TOK_EOF = 0,
    TOK_NUM,
    TOK_ID,
    TOK_STRING,
    TOK_INVALID = -1,
};
}

DIGIT [0-9]
HEXDIGIT [0-9a-fA-F]
BYTECHAR [\x00-\x7f\xa1-\xdf]{-}[\"]
WIDECHAR [\x80-\xa0\xe0-\xff]
ID [a-zA-Z_][a-zA-Z0-9_]*

%%

    /* always match strings first */
\"((\\[\"\\])|{BYTECHAR}|({WIDECHAR}(?s:.)))*\"   return TOK_STRING;
    /* kill comments */
#.*
    /* hex literals */
${HEXDIGIT}+                |
0[xX]{HEXDIGIT}+            |
    /* dec literals */
{DIGIT}+                    |
-{DIGIT}+                   return TOK_NUM;
    /* identifier */
{ID}                        return TOK_ID;
    /* kill whitespace */
[\t\v\f\r\n ]+

.   return TOK_INVALID;


