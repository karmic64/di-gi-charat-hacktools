#include <string.h>

#include "lex.h"
#include "compress.h"

enum
{
  TYPE_LINEBREAK = 0,
  TYPE_COMMENT,
  TYPE_STRING
};

typedef struct
{
  int type;
  void *data;
} entry_t;

typedef struct
{
  uint32_t offs;
  int refs;
  uint32_t *reftbl;
  int max;
} string_t;

uint8_t *rombuf = NULL;
size_t romsize;

int tocents = 0;
entry_t *toc = NULL;

entry_t *addstringref(uint32_t ref)
{
  uint32_t offs = get32(rombuf+ref)-MAPBASE;
  if (offs >= romsize) return NULL; /* ignore invalid/null pointers */
  if (*(rombuf+offs) == 0) return NULL; /* ignore null strings */
  string_t *str = NULL;
  /* check if this already exists in the toc */
  int found = 0;
  while (found < tocents)
  {
    if (toc[found].type == TYPE_STRING)
    {
      str = toc[found].data;
      if (str->offs == offs) break;
    }
    found++;
  }
  if (found < tocents)
  { /* found */
    str->reftbl = realloc(str->reftbl, (str->refs + 1)*sizeof(uint32_t));
    str->reftbl[str->refs++] = ref;
  }
  else
  { /* not found */
    toc = realloc(toc, (tocents+1)*sizeof(entry_t));
    toc[tocents].type = TYPE_STRING;
    str = malloc(sizeof(string_t));
    toc[tocents].data = str;
    tocents++;
    
    str->offs = offs;
    str->refs = 1;
    str->reftbl = malloc(sizeof(uint32_t));
    str->reftbl[0] = ref;
    str->max = -1;
  }
  return toc+found;
}

entry_t *addstringabs(uint32_t offs, int max)
{
  if (*(rombuf+offs) == 0) return NULL; /* ignore null strings */
  string_t *str = NULL;
  /* check if this already exists in the toc */
  int found = 0;
  while (found < tocents)
  {
    if (toc[found].type == TYPE_STRING)
    {
      str = toc[found].data;
      if (str->offs == offs) break;
    }
    found++;
  }
  if (found < tocents)
  { /* found */
    /* ... no references, so do nothing */
  }
  else
  { /* not found */
    toc = realloc(toc, (tocents+1)*sizeof(entry_t));
    toc[tocents].type = TYPE_STRING;
    str = malloc(sizeof(string_t));
    toc[tocents].data = str;
    tocents++;
    
    str->offs = offs;
    str->refs = 0;
    str->reftbl = NULL;
    str->max = max;
  }
  return toc+found;
}

int findmax(uint8_t *s)
{
  int m = 0;
  while (s[m] != 0) m++;
  while (s[m] == 0) m++;
  return m;
}


int main(int argc, char *argv[])
{
  FILE *f = fopen(ROMNAME, "rb");
  if (!f)
  {
    printf("Could not open ROM file: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }
  fseek(f, 0, SEEK_END);
  romsize = ftell(f);
  rewind(f);
  rombuf = malloc(romsize);
  fread(rombuf, 1, romsize, f);
  fclose(f);
  
  /* ---------- build output file TOC ---------- */
  f = fopen("stringlist.txt", "rb");
  if (!f)
  {
    printf("Could not open string list: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }
  lexinit(f, "stringlist.txt");
  int lextype = yylex();
  while (lextype)
  {
    if (lextype != TOK_ID)
    {
      err_wrongtype(TOK_ID, lextype);
      break;
    }
    else if (!strcmp("LineBreak", yytext))
    {
      toc = realloc(toc, (tocents+1)*sizeof(entry_t));
      toc[tocents].type = TYPE_LINEBREAK;
      tocents++;
      lextype = yylex();
    }
    else if (!strcmp("Comment", yytext))
    {
      lextype = yylex();
      if (lextype != TOK_STRING)
      {
        err_wrongtype(TOK_STRING, lextype);
        break;
      }
      yytext[strlen(yytext)-1] = 0;
      toc = realloc(toc, (tocents+1)*sizeof(entry_t));
      toc[tocents].type = TYPE_COMMENT;
      toc[tocents].data = yytext+1;
      tocents++;
      lextype = yylex();
    }
    else if (!strcmp("Abs", yytext))
    {
      uint32_t offs = -1;
      int max = -1;
      int fail = 0;
      while (1)
      {
        lextype = yylex();
        if (lextype == TOK_NUM)
        {
          if (offs != -1)
          {
            err("offset was already defined");
            fail++;
            break;
          }
          offs = stoi(yytext);
        }
        else if (!strcmp("Max", yytext))
        {
          lextype = yylex();
          if (lextype != TOK_NUM)
          {
            err_wrongtype(TOK_NUM, lextype);
            fail++;
            break;
          }
          if (max != -1)
          {
            err("max was already defined");
            fail++;
            break;
          }
          max = stoi(yytext);
        }
        else
        {
          break;
        }
      }
      if (fail) break;
      if (offs == -1)
      {
        err("new string definition reached but no offset defined");
        break;
      }
      if (max == -1)
      { /* find max automatically if it was not specified */
        max = findmax(rombuf+offs);
      }
      addstringabs(offs, max);
    }
    else if (!strcmp("Ptr", yytext))
    {
      while ((lextype = yylex()) == TOK_NUM)
      {
        addstringref(stoi(yytext));
      }
    }
    else if (!strcmp("StringList", yytext))
    {
      uint32_t offs = -1;
      int max = -1;
      int dist = -1;
      int amt = -1;
      int fail = 0;
      while (1)
      {
        lextype = yylex();
        if (lextype == TOK_NUM)
        {
          if (offs != -1)
          {
            err("offset was already defined");
            fail++;
            break;
          }
          offs = stoi(yytext);
        }
        else if (!strcmp("Max", yytext))
        {
          lextype = yylex();
          if (lextype != TOK_NUM)
          {
            err_wrongtype(TOK_NUM, lextype);
            fail++;
            break;
          }
          if (max != -1)
          {
            err("max was already defined");
            fail++;
            break;
          }
          max = stoi(yytext);
        }
        else if (!strcmp("Dist", yytext))
        {
          lextype = yylex();
          if (lextype != TOK_NUM)
          {
            err_wrongtype(TOK_NUM, lextype);
            fail++;
            break;
          }
          if (dist != -1)
          {
            err("dist was already defined");
            fail++;
            break;
          }
          dist = stoi(yytext);
        }
        else if (!strcmp("Amt", yytext))
        {
          lextype = yylex();
          if (lextype != TOK_NUM)
          {
            err_wrongtype(TOK_NUM, lextype);
            fail++;
            break;
          }
          if (amt != -1)
          {
            err("amt was already defined");
            fail++;
            break;
          }
          amt = stoi(yytext);
        }
        else
        {
          break;
        }
      }
      if (fail) break;
      if (offs == -1)
      {
        err("new string definition reached but no offset defined");
        break;
      }
      if (max == -1 && dist == -1)
      {
        err("new string definition reached but no max or dist defined");
        break;
      }
      else if (max == -1)
      {
        max = dist;
      }
      else if (dist == -1)
      {
        dist = max;
      }
      if (amt == -1)
      {
        err("new string definition reached but no amt defined");
        break;
      }
      for (int i = 0; i < amt; i++)
      {
        addstringabs(offs+(dist*i), max);
      }
    }
    else if (!strcmp("PtrList", yytext))
    {
      uint32_t offs = -1;
      int amt = -1;
      int fail = 0;
      while (1)
      {
        lextype = yylex();
        if (lextype == TOK_NUM)
        {
          if (offs != -1)
          {
            err("offset was already defined");
            fail++;
            break;
          }
          offs = stoi(yytext);
        }
        else if (!strcmp("Amt", yytext))
        {
          lextype = yylex();
          if (lextype != TOK_NUM)
          {
            err_wrongtype(TOK_NUM, lextype);
            fail++;
            break;
          }
          if (amt != -1)
          {
            err("amt was already defined");
            fail++;
            break;
          }
          amt = stoi(yytext);
        }
        else
        {
          break;
        }
      }
      if (fail) break;
      if (offs == -1)
      {
        err("new string definition reached but no offset defined");
        break;
      }
      if (amt == -1)
      {
        err("new string definition reached but no amt defined");
        break;
      }
      for (int i = 0; i < amt; i++)
      {
        addstringref(offs+(4*i));
      }
    }
    else
    {
      err("invalid identifier %s", yytext);
      break;
    }
    
    
  }
  fclose(f);
  
  /* ---------- create output file from TOC ----------- */
  f = fopen("text-orig.txt", "wb");
  FILE *df = fopen("textrawdump.txt", "wb");
  
  int strcnt = 0;
  for (int i = 0; i < tocents; i++)
  {
    void *data = toc[i].data;
    switch (toc[i].type)
    {
      case TYPE_LINEBREAK:
      {
        fputc('\n', f);
        break;
      }
      case TYPE_COMMENT:
      {
        fprintf(f, "### %s\n", (char*)data);
        break;
      }
      case TYPE_STRING:
      {
        string_t *str = data;
        /* insert original text as a comment */
        fprintf(f, "# ");
        uint8_t *strdata = rombuf + str->offs;
        uint8_t c;
        while ((c = *(strdata++)))
        {
          if (c == '\n')
          {
            fprintf(f, "\\n");
            fprintf(df, "\\n");
          }
          else if (c < 0x20)
          {
            fprintf(f, "\\x%02x", c);
            fprintf(df, "\\x%02x", c);
          }
          else
          {
            fputc(c, f);
            fputc(c, df);
          }
        }
        fputc('\n', f);
        fputc('\n', df);
        /* now put in actual commands */
        int refs = str->refs;
        if (refs)
        { /* has references, use Ptr */
          fprintf(f, "Ptr ");
          for (int r = 0; r < refs; r++)
          {
            fprintf(f, "$%X ", str->reftbl[r]);
          }
        }
        else
        { /* no references, use Abs */
          fprintf(f, "Abs $%X Max %i ", str->offs, str->max);
        }
        fprintf(f, "\"!! %i\"\n", strcnt++);
      }
    }
  }
  
  fclose(df);
  fclose(f);
  
  
}


