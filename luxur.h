/* luxur.h - Free Single Header C89 Lexer Library
    VERSION
0.5

    HOW TO USE
To define functions as static:
 #define X_DECL static
 #include "luxur.h"

To include the implementation:
 #define LUXUR_C
 #include "luxur.h"

To specify the mode:
 #define LUXUR_C
 #define X_MODE_B_LANGUAGE
 #include "luxur.h"
where C language is the mode by default.

    LICENSE
See end of file for license information.
**************************************************************************************************/
/* HEADER                                                                                        */
/*************************************************************************************************/
#ifndef LUXUR_H
#define LUXUR_H
/*************************************************************************************************/
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*************************************************************************************************/
#define X_RESET    "\033[0m"
#define X_BOLD     "\033[1m"
#define X_BRED     "\033[1;31m"
#define X_BMAGENTA "\033[1;35m"
#define X_BCYAN    "\033[1;36m"
/*************************************************************************************************/
#ifndef X_DECL
#define X_DECL
#endif
/*************************************************************************************************/
#define X_EOF 0
/*************************************************************************************************/
#ifdef __GNUC__
#define X_PRINTF(formati, argsi) __attribute__ ((format (printf, formati, argsi)))
#else
#define X_PRINTF(formati, argsi)
#endif /* __GNUC__ */
/*************************************************************************************************/
enum { X_NOTE, X_WARN, X_ERR, X_FERR, X_MTYPE_COUNT };
/*************************************************************************************************/
typedef unsigned int X_Uint32;
typedef unsigned long X_Uint64;
typedef X_Uint32 X_Token;
/*************************************************************************************************/
#define         x_accept(selector) xz_scan(1 | 0, (selector))
X_DECL char    *x_decode(X_Token quoted, int *size_in_out);
X_DECL int      x_eof(void);
#define         x_expect(selector) xz_scan(1 | 2, (selector))
X_DECL int      x_equals(X_Token t1, X_Token t2);
X_DECL int      x_equals_mem(X_Token token, X_Uint32 size, const void *data);
X_DECL void     x_init(int filename_length, const char *filename);
X_DECL int      x_isspace(int ch);
X_DECL void     x_msgf(X_Uint32 type, X_Token token, const char *format, ...) X_PRINTF(3, 4);
X_DECL X_Uint64 x_parse64(X_Token number);
X_DECL X_Token  x_read(void);
#define         x_toklen(token) (((token) >> 24) & 0xFF)
#define         x_tokstr(token) (xz_src + ((token) & 0xFFFFFF))
#define         x_verify(selector) xz_scan(0 | 0, (selector))
/*************************************************************************************************/
X_DECL X_Token xz_read(void);
X_DECL X_Token xz_scan(X_Uint32 mask, const char *selector);
/*************************************************************************************************/
#endif /* LUXUR_H */
/*************************************************************************************************/
/* SOURCE                                                                                        */
/*************************************************************************************************/
#ifdef LUXUR_C
#ifndef LUXUR_C_INCLUDED
#define LUXUR_C_INCLUDED
/*************************************************************************************************/
#if defined(X_MODE_B_LANGUAGE)
#define X_ESCAPE_CHAR '*'
#else /* X_MODE_ENDER_C, X_MODE_ENDER_ASM, or C */
#define X_ESCAPE_CHAR '\\'
#endif
/*************************************************************************************************/
static char *xz_filename;
static int xz_filename_length;
static X_Token xz_previous_token, xz_current_token;
static char xz_src[0x1000000]; /* 16M */
/*************************************************************************************************/
#define xz_toknew(offset, length) ((offset) | ((length) << 24))
/*************************************************************************************************/
char *x_decode(X_Token quoted, int *size_in_out)
{
  static char escmap[] =
#if defined(X_MODE_B_LANGUAGE)
  "**n\nt\tr\re\033({)}\'\'\"\"";
#else /* X_MODE_ENDER_C, X_MODE_ENDER_ASM, or C */
  "\\\\n\nt\tr\rv\va\ae\033\'\'\"\"";
#endif

  static unsigned char string[0x1001];
  
  char *cp, *end;
  int size, max_size;
  X_Uint32 ch;
  
  max_size = (int)sizeof(string) - 1;
  size = *size_in_out;
  
  cp = x_tokstr(quoted);
  end = cp + x_toklen(quoted);
  
  ++cp;
  --end;
  
#if defined(X_MODE_ENDER_C) || defined(X_MODE_ENDER_ASM)
  if (cp[-1] == '`')
  {
    void *dst;
    int len;
  
    len = end - cp;
    dst = string + size;
    size += len;
    
    if (size > max_size)
    {
      x_msgf(X_FERR, quoted, "the text is bigger than %i bytes", max_size);
    }
    
    memcpy(dst, cp, len);
    goto string_ready;
  }
#endif /* X_MODE_ENDER_C */
  
  for (; cp != end; string[size++] = ch)
  {
    if (size == max_size)
    {
      x_msgf(X_FERR, quoted, "the text is bigger than %i bytes", max_size);
    }
  
    if (*cp == X_ESCAPE_CHAR)
    {
      char *kv;
    
      ++cp;

      for (kv = escmap; *kv != 0; kv += 2)
      {
        if (*cp == kv[0])
        {
          ch = kv[1];
          ++cp;
          break;
        }
      }
      
      if (*kv != 0)
      {
        continue;
      }
      
      if ('0' <= *cp && *cp <= '7')
      {
        ch = 0;
      
        for (; '0' <= *cp && *cp <= '7'; ++cp)
        {
          ch *= 8;
          ch += *cp - '0';
        }
        
        continue;
      }
      
      if (*cp == 'x' || *cp == 'X')
      {
        char offset;
      
        ++cp;
        
        ch = 0;
      
        for (; isxdigit(*cp); ++cp)
        {
          ch *= 16;
          
          if (isdigit(*cp))
          {
            offset = -'0';
          }
          else if (isupper(*cp))
          {
            offset = -'A' + 10;
          }
          else if (islower(*cp))
          {
            offset = -'a' + 10;
          }
          
          ch += *cp + offset;
        }
        
        continue;
      }
    }
    else
    {
      ch = *cp;
      ++cp;
    }
  }

#if defined(X_MODE_ENDER_C) || defined(X_MODE_ENDER_ASM)
string_ready:
#endif

  string[size] = '\0';
  *size_in_out = size;
  return (char *)string;
}
/*************************************************************************************************/
int x_eof(void)
{
  return (xz_current_token == X_EOF);
}
/*************************************************************************************************/
int x_equals(X_Token t1, X_Token t2)
{
  int len1, len2;
  
  return x_equals_mem(t1, x_toklen(t2), x_tokstr(t2));
  
  len1 = x_toklen(t1);
  len2 = x_toklen(t2);
  
  return (len1 == len2 && memcmp(x_tokstr(t1), x_tokstr(t2), len1) == 0);
}
/*************************************************************************************************/
int x_equals_mem(X_Token token, X_Uint32 size, const void *data)
{
  return (x_toklen(token) == size && memcmp(x_tokstr(token), data, size) == 0);
}
/*************************************************************************************************/
void x_init(int filename_length, const char *filename)
{
  char *copied_filename;
  FILE *stream;

  xz_filename = (char *)filename;
  xz_filename_length = filename_length;
  
  copied_filename = calloc(filename_length + 1, 1);
  
  if (copied_filename == NULL)
  {
    x_msgf(X_FERR, 0, "%s", strerror(errno));
  }
  
  memcpy(copied_filename, filename, filename_length);
  xz_filename = copied_filename;
  
  stream = fopen(xz_filename, "rb");
  
  if (stream == NULL)
  {
    x_msgf(X_FERR, 0, "%s", strerror(errno));
  }
  
  fread(xz_src, 1, sizeof(xz_src) - 1, stream);
  
  fclose(stream);
  
  xz_current_token = xz_read();
}
/*************************************************************************************************/
int x_isspace(int ch)
{
#if defined(X_MODE_ENDER_ASM)
  return '\0' < ch && ch <= ' ' && ch != '\n';
#else /* X_MODE_B_LANGUAGE, X_MODE_ENDER_C, or C */
  return isspace(ch);
#endif
}
/*************************************************************************************************/
void x_msgf(X_Uint32 type, X_Token token, const char *format, ...)
{
  static const char *colors[X_MTYPE_COUNT] = { X_BCYAN, X_BMAGENTA, X_BRED,  X_BRED };
  static const char *titles[X_MTYPE_COUNT] = { "note",  "warning",  "error", "fatal error" };
  
  va_list args;
  
  if (token == 0)
  {
    if (xz_filename != NULL)
    {
      fprintf(stderr, X_BOLD"%.*s: "X_RESET, xz_filename_length, xz_filename);
    }
  
    fprintf(stderr, "%s%s:"X_RESET" ", colors[type], titles[type]);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
  }
  else
  {
    char *line_view, *cp, *value;
    int row, col, rowspaces, before, length, after, line_length;
  
    value = x_tokstr(token);
    length = x_toklen(token);
    line_view = xz_src;
    row = 1;
  
    for (cp = xz_src; cp != value; ++cp)
    {
      if (*cp == '\n')
      {
        line_view = cp + 1;
        row += 1;
      }
    }
    
    for (line_length = 0; line_view[line_length] != '\0'
                       && line_view[line_length] != '\n'; ++line_length) /**/;
    
    col = value - line_view + 1;
    
    fprintf(stderr, X_BOLD"%.*s:%i:%i: "X_RESET"%s%s:"X_RESET" ",
      xz_filename_length, xz_filename, row, col, colors[type], titles[type]);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    
    rowspaces = fprintf(stderr, " %4i ", row);
    
    before = value - line_view;
    after = line_length - before - length;
    
    fprintf(stderr, "| %.*s%s%.*s"X_RESET"%.*s\n",
      before, line_view, colors[type], length, value, after, line_view + before + length);
    
    for (; rowspaces > 0; --rowspaces)
    {
      fputc(' ', stderr);
    }
    
    fprintf(stderr, "| ");
    
    for (; before > 0; --before)
    {
      fputc(' ', stderr);
    }
    
    fprintf(stderr, "%s", colors[type]);
    fputc('^', stderr);
    
    for (--length; length > 0; --length)
    {
      fputc('~', stderr);
    }
    
    fprintf(stderr, "%s\n", X_RESET);
  }

  if (type == X_FERR)
  {
    exit(EXIT_FAILURE);
  }
}
/*************************************************************************************************/
X_Uint64 x_parse64(X_Token number)
{
  X_Uint64 value, base, digit;
  char *cp, *end;
  int length;

  cp = x_tokstr(number);
  length = x_toklen(number);
  end = cp + length;
  value = 0;
  base = 10;
  
  if (length > 1 && cp[0] == '0')
  {
    /**/ if (cp[1] == 'x' || cp[1] == 'X')
    {
      base = 16;
      cp += 2;
    }
    else if (cp[1] == 'b' || cp[1] == 'B')
    {
      base = 2;
      cp += 2;
    }
    else if (cp[1] == 'z' || cp[1] == 'Z')
    {
      base = 32;
      cp += 2;
    }
    else
    {
      base = 8;
      cp += 1;
    }
    length = end - cp;
  }
  
  if (length == 0)
  {
    x_msgf(X_ERR, number, "invalid integer constant");
    return 0;
  }
  
  for (; cp != end; ++cp)
  {
    /**/ if ('0' <= *cp && *cp <= '9')
    {
      digit = *cp - '0';
    }
    else if ('A' <= *cp && *cp <= 'Z')
    {
      digit = *cp - 'A' + 10;
    }
    else if ('a' <= *cp && *cp <= 'z')
    {
      digit = *cp - 'a' + 10;
    }
    else
    {
      digit = 99;
    }
    
    if (digit >= base)
    {
      x_msgf(X_ERR, number, "invalid integer constant");
      return 0;
    }
    
    value *= base;
    value += digit;
  }

  return value;
}
/*************************************************************************************************/
X_Token x_read(void)
{
  xz_previous_token = xz_current_token;
  xz_current_token = xz_read();
  return xz_previous_token;
}
/*************************************************************************************************/
X_Token xz_read(void)
{
  static const char ops[] =
#if defined(X_MODE_B_LANGUAGE)
  "3=<<3=>>3=<=3=>=3===3=!=2=+2=-2=*2=/2=%2=&2=^2=|2=>2=<2<<2>>2++2--2<=2>=2==2!=1*1/1%1+1-1^1&1|"
  "1<1>1?1:1=1,1!1~1(1)1[1]1{1}1;";
#elif defined(X_MODE_ENDER_C)
  "3|<=3|>=2+=2-=2*=2/=2%=2><2<=2>=2==2!=2|<2|>2++2--2&&2||1*1/1%1+1-1=1<1>1,1!1(1)1[1]1{1}1;1?1:";
#elif defined(X_MODE_ENDER_ASM)
  "2<=2>=2==2!=2|<2|>2<<2>>2&&2||1\n1=1*1/1%1+1-1&1|1^1~1<1>1,1!1(1)1{1}1;1?1:";
#else /* C */
  "3<<=3>>=2+=2-=2*=2/=2%=2&=2^=2|=2<=2>=2==2!=2<<2>>2++2--2->2&&2||1*1/1%1+1-1^1&1|1<1>1?1:1=1,1!"
  "1~1(1)1[1]1{1}1.1#1;";
#endif
  static X_Uint32 ci;
  
  const char *op;
  X_Uint32 offset, length, oplen;
  
  for (;;)
  {
    for (; x_isspace(xz_src[ci]); ++ci) /**/;
    
    if (xz_src[ci] == '/' && xz_src[ci + 1] == '*')
    {
      int first_newline;
    
      first_newline = 0;
      offset = ci;
      ci += 2;
    
      for (; xz_src[ci] != '\0' && !(xz_src[ci] == '*' && xz_src[ci + 1] == '/'); ++ci)
      {
        if (first_newline == 0 && xz_src[ci] == '\n')
        {
          first_newline = ci + 1;
        }
      }
      
      if (xz_src[ci] == '\0')
      {
        if (first_newline == 0)
        {
          first_newline = ci;
        }
      
        x_msgf(X_FERR, xz_toknew(offset, first_newline - offset - 1), "endless comment");
      }
      
      ci += 2;
    
      continue;
    }
    
    if (xz_src[ci] == '/' && xz_src[ci + 1] == '/')
    {
      for (; xz_src[ci] != '\0' && xz_src[ci] != '\n'; ++ci) /**/;
    
      continue;
    }
    
    break;
  }
  
  offset = ci;
  
  if (xz_src[ci] == '\0')
  {
    return X_EOF;
  }
  
  if (isalnum(xz_src[ci]) || xz_src[ci] == '_')
  {
    for (; isalnum(xz_src[ci]) || xz_src[ci] == '_'; ++ci) /**/;
  
    goto token_ready;
  }
  
#if defined(X_MODE_ENDER_C) || defined(X_MODE_ENDER_ASM)
  if (xz_src[ci] == '`')
  {
    ++ci;
    
    for (; xz_src[ci] != '\0' && xz_src[ci] != '\n' && xz_src[ci] != '`'; ++ci) /**/;
    
    if (xz_src[ci] != '`')
    {
      x_msgf(X_FERR, xz_toknew(offset, ci - offset), "endless quoted text");
    }
    
    ++ci;
    
    goto token_ready;
  }
#endif /* X_MODE_ENDER_C */
  
  if (xz_src[ci] == '"' || xz_src[ci] == '\'')
  {
    int quote, n;
    
    quote = xz_src[ci];
    n = 0;
    
    ++ci;
    
    for (; (xz_src[ci] != '\0' && xz_src[ci] != '\n' && xz_src[ci] != quote) || n % 2 == 1;)
    {
      n = 0;
      if (xz_src[ci] == X_ESCAPE_CHAR)
      {
        for (; xz_src[ci] == X_ESCAPE_CHAR; ++ci, ++n) /**/;
      }
      else
      {
        ++ci;
      }
    }
    
    if (xz_src[ci] != quote)
    {
      x_msgf(X_FERR, xz_toknew(offset, ci - offset), "endless quoted text");
    }
    
    ++ci;
    
    goto token_ready;
  }
  
  for (op = ops; *op != '\0'; op += oplen)
  {
    oplen = *op - '0';
    ++op;
    
    if (memcmp(xz_src + ci, op, oplen) == 0)
    {
      ci += oplen;
      goto token_ready;
    }
  }
  
  x_msgf(X_FERR, xz_toknew(offset, 1), "invalid token");
  
  return X_EOF;
  
token_ready:
  
  length = ci - offset;
  
  if (length > 255)
  {
    x_msgf(X_FERR, xz_toknew(offset, 255), "the token is too long");
  }
  
  return xz_toknew(offset, length);
}
/*************************************************************************************************/
X_Token xz_scan(X_Uint32 mask, const char *selector)
{
  char *value;
  int length, slength, ok;
  
  value = x_tokstr(xz_current_token);
  length = x_toklen(xz_current_token);
  slength = strlen(selector);
  
  if (selector[0] == ' ')
  {
    switch (selector[1])
    {
    case 'A': ok = (isalpha(value[0]) || value[0] == '_'); break;
    case 'D': ok = isdigit(value[0]); break;
    case 'Q': ok = (value[0] == '`' || value[0] == '"' || value[0] == '\''); break;
    default: ok = (value[0] == selector[1]); break;
    }
  }
  else
  {
    ok = (length == slength && memcmp(value, selector, length) == 0);
  }
  
  if ((mask & 2) && !ok)
  {
    if (xz_current_token != X_EOF)
    {
      x_msgf(X_FERR, xz_current_token, "unexpected token");
    }
    else
    {
      x_msgf(X_FERR, xz_previous_token, "unexpected end of file");
    }
  }
  
  if ((mask & 1) && ok)
  {
    return x_read();
  }

  return (ok) ? xz_current_token : 0;
}
/*************************************************************************************************/
#endif /* LUXUR_C_INCLUDED */
#endif /* LUXUR_C */
/*************************************************************************************************/
/* LICENSE                                                                                       */
/**************************************************************************************************
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>
**************************************************************************************************/

