#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>

#include "../lex.yy.c"



int main(int argc, char* argv[])
{
    
    FILE *tf = fopen("text-translated.txt", "rb");
    FILE *infile = fopen("../text-orig.txt", "rb");
    FILE *outfile = fopen("../text.txt", "wb");
    
    yyrestart(infile);
    int lextype;
    while (lextype = yylex())
    {
        if (lextype != TOK_STRING)
        {
            fprintf(outfile, "%s ", yytext);
        }
        else
        {
            fputc('\"', outfile);
            uint8_t strbuf[0x400];
            fgets(strbuf, 0x400, tf);
            int i = 0;
            uint8_t c;
            while (c = strbuf[i++])
            {
                if (c == '\n') break;
                if (c == '%' && strbuf[i] == ' ')
                {
                    if (i != 1)
                        fputc(' ', outfile);
                    fputc('%', outfile);
                    i++;
                }
                else if (c == '\\' && strbuf[i] == ' ')
                {
                    fputc('\\', outfile);
                    i++;
                }
                else if (c == '"')
                {
                    fputc('\\', outfile);
                    fputc('"', outfile);
                }
                else
                {
                    fputc(c, outfile);
                }
            }
            fprintf(outfile, "\" ");
        }
    }
    
    fclose(tf);
    fclose(infile);
    fclose(outfile);
}