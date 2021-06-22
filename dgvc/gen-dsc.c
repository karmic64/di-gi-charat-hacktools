#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>

#include "lex.yy.c"



int main(int argc, char* argv[])
{
    mkdir("../DSC-new");
    
    FILE *tf = fopen("dump-translated.txt", "rb");
    
    int status = chdir("../DSC-orig");
    if (status)
    {
        printf("Could not change to DSC directory: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    uint8_t *strbuf = malloc(0x400);
    int endflag = 0;
    while (!endflag)
    {
        
        FILE *infile = NULL;
        FILE *outfile = NULL;
        
        if (feof(tf))
        {
            endflag++;
            goto end;
        }
        
        char b[64];
        fgets(b, 64, tf);
        int status = sscanf(b, "NEWFILE %s", strbuf);
        if (!status)
        {
            printf("Invalid string \"%s\" in text file\n", b);
            break;
        }
        
        infile = fopen((char*)strbuf, "rb");
        if (!infile)
        {
            printf("Could not open %s: %s\n", strbuf, strerror(errno));
            break;
        }
        yyrestart(infile);
        yylineno = 1;
        
        sprintf(b, "../DSC-new/%s", strbuf);
        outfile = fopen(b, "wb");
        
        int lextype;
        while ((lextype = yylex()))
        {
            switch (lextype)
            {
                case TOK_INVALID:
                    printf("Parse error in %s, \"%s\"\n", strbuf, yytext);
                    goto fail;
                case TOK_STRING:
                {
                    fputc('\"', outfile);
                    int i = 0;
                    while (1)
                    {
                        int c = fgetc(tf);
                        if (c == EOF) break;
                        if (c == '\n') break;
                        strbuf[i++] = c;
                        if (c >= 0x80)
                        {
                            strbuf[i++] = fgetc(tf);
                        }
                    }
                    strbuf[i] =  '\0';
                    int len = i - 1;
                    i = 0;
                    while (i < len)
                    {
                        /* clean up */
                        if (strbuf[i] == '\\' && strbuf[i+1] == ' ')
                        {
                            memmove(strbuf+i+1, strbuf+i+2, len-i-2);
                            len--;
                            strbuf[len] = '\0';
                        }
                        if (strbuf[i] == '$' && strbuf[i+1] == ' ')
                        {
                            fputc('$', outfile);
                            i += 2;
                        }
                        /* now fix errors */
                        else if (strbuf[i] == ' ' && strbuf[i+1] == 0x81 && strbuf[i+2] == 0x66)
                        { /* weird shift-jis apostrophe */
                            fputc('\'', outfile);
                            i += 3;
                        }
                        else if (strbuf[i] == 0x81 && strbuf[i+1] == 0x66)
                        {
                            fputc('\'', outfile);
                            i += 2;
                        }
                        else if (strbuf[i] == '\\' && strbuf[i+1] == 'X' && isxdigit(strbuf[i+2]) && isxdigit(strbuf[i+3]))
                        {
                            fputc('\\', outfile);
                            fputc('x', outfile);
                            fputc(strbuf[i+2], outfile);
                            fputc(strbuf[i+3], outfile);
                            if (strbuf[i-1] == ' ' && strbuf[i+4] == ' ')
                            {
                                i++;
                            }
                            i += 4;
                        }
                        else if (strbuf[i] == '\\')
                        {
                            switch (strbuf[i+1])
                            {
                                case '\0':
                                    fputc('\\', outfile);
                                    fputc('\"', outfile);
                                    i += 2;
                                    break;
                                case 'x':
                                    fputc('\\', outfile);
                                    fputc(strbuf[i+1], outfile);
                                    fputc(strbuf[i+2], outfile);
                                    fputc(strbuf[i+3], outfile);
                                    if (strbuf[i-1] == ' ' && strbuf[i+4] == ' ')
                                    {
                                        i++;
                                    }
                                    i += 4;
                                    break;
                                case 'n':
                                case 'J':
                                    fputc('\\', outfile);
                                    fputc(strbuf[i+1], outfile);
                                    i += 2;
                                    break;
                                default:
                                    fputc('\\', outfile);
                                    fputc('\\', outfile);
                                    i++;
                                    break;
                            }
                            
                        }
                        /* otherwise print as normal */
                        else if (strbuf[i] >= 0x80)
                        {
                            fputc(strbuf[i++], outfile);
                            fputc(strbuf[i++], outfile);
                        }
                        else if (strbuf[i] == '\"')
                        {
                            fputc('\\', outfile);
                            fputc('\"', outfile);
                            i++;
                        }
                        else
                        {
                            fputc(strbuf[i++], outfile);
                        }
                    }
                    fputc('\"', outfile);
                    fputc('\n', outfile);
                    break;
                }
                default:
                    fprintf(outfile, "%s ", yytext);
                    break;
            }
        }
end:    
        if (infile) fclose(infile);
        if (outfile) fclose(outfile);
    }
fail:
    free(strbuf);
    fclose(tf);
}