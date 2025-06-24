/* enderasm.c - EnderASM
LICENSE: See end of file for license information.
BUILD: $ cc -std=c89 -pedantic -pedantic-errors -o enderasm enderasm.c
USAGE: $ enderasm [options] <file.s>

  PLAN FOR V1.1.0
TODO: warning about unreachable code
TODO: "print" pseudo-instruction like "cmd" but generates "tellraw" command automatically
TODO: alias my_foo = "nmsp:foreign"
TODO: arrays or/and pointers
**************************************************************************************************/
/* HEADER                                                                                        */
/*************************************************************************************************/
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*************************************************************************************************/
#define X_MODE_ENDER_ASM
#define LUXUR_C
#include "luxur.h"
/*************************************************************************************************/
#define numof(array) (sizeof(array)/sizeof((array)[0]))
/*************************************************************************************************/
typedef signed char   i8; typedef signed short   i16; typedef signed int   i32;
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
/*************************************************************************************************/
typedef unsigned long u64;
typedef signed long   i64;
/*************************************************************************************************/
typedef struct Operator {
  const char *o_selector;
  u32 o_lbp;
  u32 o_rbp;
} Operator;
/*************************************************************************************************/
enum /* Es_Branch_Type */ { ES_JL, ES_JLE, ES_JE, ES_JNE, ES_JG, ES_JGE, ES_JCOUNT };
/*************************************************************************************************/
enum /* Es_Symbol_Kind */ { ES_SYM_CONSTANT, ES_SYM_LABEL, ES_SYM_ETERNAL, ES_SYM_COUNT };
enum /* Es_Symbol_Flags */ { ES_SYMF_DEFINED = 1, ES_SYMF_USED = 2, ES_SYMF_EXTERN = 4 };
/*************************************************************************************************/
typedef struct Es_Symbol {
  u16 st_kind;
  u16 st_flags;
  X_Token st_name;
  union {
    i64 as_i64;
    u64 as_u64;
    u32 as_address;
    u32 as_index;
  } st_value;
} Es_Symbol;
/*************************************************************************************************/
typedef struct Es_Defined_Label {
  X_Token dl_name;
  u32 dl_text_start;
  u32 dl_text_end;
} Es_Defined_Label;
/*************************************************************************************************/
enum /* Es_Node_Kind */ { ES_NODE_NAME, ES_NODE_INTEGER, ES_NODE_CHARACTER, ES_NODE_BINARY,
  ES_NODE_PREFIX };
/*************************************************************************************************/
typedef struct Es_Node {
  X_Token n_token;
  u8 n_kind;
  u8 n_lhs;
  u8 n_rhs;
} Es_Node;
/*************************************************************************************************/
static i64   es_expression_evaluate(u32 ni);
static u32   es_expression_parse_bp(u32 min_bp);
static void  es_generate_branch(u32 type);
static void  es_generate_jump(X_Token label_name);
static void  es_generate_operation(const char *opcode, const char *altcode, i32 stupid_bug);
static void  es_instruction_argument(i32 *constant_out, i32 *si_out);
static i32   es_labels_define(X_Token name);
static void  es_labels_end_active_ones(void);
static u32   es_nodes_create_one(X_Token token, u8 kind, u8 lhs, u8 rhs);
static const Operator *es_operator_find(const Operator *tab, u32 num);
static i32   es_symbol_declare(u16 kind, u16 flags, X_Token name);
static void  es_symbol_eternal_print_name(i32 si);
static i32   es_symbol_find(X_Token name);
static FILE *xopen_memstream(char **ptr, size_t *sizeloc);
/*************************************************************************************************/
/* SOURCE                                                                                        */
/*************************************************************************************************/
static const char *es_symbol_title[ES_SYM_COUNT] ={"a constant", "a label", "an eternal variable"};
static char *es_output_directory = ".";
static char *es_mcfunction_prefix = "";
static char *es_objective_name = "INT";
static char *es_eternal_prefix = "_";
/*************************************************************************************************/
static i32 es_add_comment_for_every_label;
/*************************************************************************************************/
static FILE  *es_cmdout;
static char  *es_cmdout_buffer;
static size_t es_cmdout_size;
/*************************************************************************************************/
static FILE  *es_symtab_stream;
static char  *es_symtab_buffer;
static size_t es_symtab_size;
#define       es_symtab_num    (es_symtab_size / sizeof(Es_Symbol))
#define       es_symtab        ((Es_Symbol *)es_symtab_buffer)
/*************************************************************************************************/
static FILE  *es_labels_stream;
static char  *es_labels_buffer;
static size_t es_labels_size;
#define       es_labels_num    (es_labels_size / sizeof(Es_Defined_Label))
#define       es_labels        ((Es_Defined_Label *)es_labels_buffer)
static size_t es_labels_active_start;
/*************************************************************************************************/
static u32 es_next_address;
/*************************************************************************************************/
static Es_Node es_nodes[256];
static u32     es_nodes_top;
/*************************************************************************************************/
i32 main(i32 argc, char *argv[])
{
  const char *source_path;
  u32 i;
  i32 k;
  
  /************************************/
  /*-------/  READ ARGUMENTS  \-------*/
  /************************************/

  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
  {
    printf("Usage: enderasm [options] <file.s>\n"
           "Options:\n"
           "  -h                       Or\n"
           "  --help                   Display this information.\n"
           "  -v                       Or\n"
           "  --version                Display compiler version information.\n"
           "  -o <directory>           Or\n"
           "  --output <directory>     Place the output into <directory>.\n"
           "  -p <string>              Or\n"
           "  --prefix <string>        Define namespace and subfolders for mcfunctions.\n");
    printf("  -l                       Or\n"
           "  --label-comments         Generate a comment in place of each label.\n"
           "\n"
    );
    exit(EXIT_SUCCESS);
  }
  
  if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
  {
    printf("enderasm 1.0.4\n"
           "This is free and unencumbered software released into the public domain.\n"
           "For more information, please refer to <https://unlicense.org/>\n\n");
    exit(EXIT_SUCCESS);
  }
  
  source_path = NULL;
  
  for (k = 1; k < argc; ++k)
  {
    /**/ if (strcmp(argv[k], "-o") == 0 || strcmp(argv[k], "--output") == 0)
    {
      ++k;
      
      if (k >= argc)
      {
        x_msgf(X_FERR, 0, "invalid '%s' option", argv[k - 1]);
      }
      
      {
        u32 len;
        
        es_output_directory = argv[k];
        len = strlen(es_output_directory);
        
        if (es_output_directory[len - 1] == '/')
        {
          es_output_directory[len - 1] = '\0'; /* can I? */
        }
      }
    }
    else if (strcmp(argv[k], "-p") == 0 || strcmp(argv[k], "--prefix") == 0)
    {
      ++k;
      
      if (k >= argc)
      {
        x_msgf(X_FERR, 0, "invalid '%s' option", argv[k - 1]);
      }

      es_mcfunction_prefix = argv[k];
    }
    else if (strcmp(argv[k], "-l") == 0 || strcmp(argv[k], "--comment-labels") == 0)
    {
      es_add_comment_for_every_label = 1;
    }
    else
    {
      source_path = argv[k];
    }
  }
  
  if (source_path == NULL)
  {
    x_msgf(X_FERR, 0, "no input file");
  }
  
  /***********************************/
  /*-------/   INIT THINGS   \-------*/
  /***********************************/
  
  es_cmdout = xopen_memstream(&es_cmdout_buffer, &es_cmdout_size);
  es_symtab_stream = xopen_memstream(&es_symtab_buffer, &es_symtab_size);
  es_labels_stream = xopen_memstream(&es_labels_buffer, &es_labels_size);
  
  x_init(strlen(source_path), source_path);
  
  es_symbol_declare(ES_SYM_ETERNAL, ES_SYMF_DEFINED | ES_SYMF_USED, 0);
  es_symtab[0].st_value.as_address = 0;
  
  /************************************/
  /*-------/  THE GREAT LOOP  \-------*/
  /************************************/
  
  while (!x_eof())
  {
    /**/ if (x_accept("\n")) /* nope */;
    else if (x_accept("mov")) es_generate_operation("=",  "set",    0);
    else if (x_accept("add")) es_generate_operation("+=", "add",    1);
    else if (x_accept("sub")) es_generate_operation("-=", "remove", 1);
    else if (x_accept("mul")) es_generate_operation("*=", NULL,     0);
    else if (x_accept("div")) es_generate_operation("/=", NULL,     0);
    else if (x_accept("mod")) es_generate_operation("%=", NULL,     0);
    else if (x_accept("min")) es_generate_operation("<",  NULL,     0);
    else if (x_accept("max")) es_generate_operation(">",  NULL,     0);
    else if (x_accept("jl"))  es_generate_branch(ES_JL);
    else if (x_accept("jle")) es_generate_branch(ES_JLE);
    else if (x_accept("jg"))  es_generate_branch(ES_JG);
    else if (x_accept("jge")) es_generate_branch(ES_JGE);
    else if (x_accept("je"))  es_generate_branch(ES_JE);
    else if (x_accept("jne")) es_generate_branch(ES_JNE);
    else if (x_accept("cmd"))
    {
      do
      {
        X_Token quoted;
        i32 si;
      
        /**/ if (x_accept("tellraw"))
        {
          es_instruction_argument(NULL, &si);
        
          fprintf(es_cmdout, "{\"score\":{\"name\":\"");
          es_symbol_eternal_print_name(si);
          fprintf(es_cmdout, "\",\"objective\":\"%s\"}}", es_objective_name);
        }
        else if ((quoted = x_accept(" Q")))
        {
          char *data;
          i32 size;
        
          size = 0;
          data = x_decode(quoted, &size);
          fwrite(data, 1, size, es_cmdout);
        }
        else
        {
          i32 constant_value, si;
          
          es_instruction_argument(&constant_value, &si);
          
          if (si < 0)
          {
            fprintf(es_cmdout, "%i", (i32)constant_value);
          }
          else
          {
            es_symbol_eternal_print_name(si);
            fprintf(es_cmdout, " %s", es_objective_name);
          }
        }
      }
      while (x_accept(","));
      
      x_expect("\n");
      
      
      fputc('\n', es_cmdout);
      fflush(es_cmdout);
    }
    else if (x_verify("extern") || x_verify("eter"))
    {
      u32 kind, flags, si;
    
      flags = x_accept("extern") ? ES_SYMF_EXTERN : 0;
      
      if (x_accept("eter"))
      {
        flags |= ES_SYMF_DEFINED;
        kind = ES_SYM_ETERNAL;
      }
      else
      {
        x_expect("func");
        kind = ES_SYM_LABEL;
      }
      
      do
      {
        si = es_symbol_declare(kind, flags, x_expect(" A"));
        
        if (kind == ES_SYM_ETERNAL)
        {
          es_symtab[si].st_value.as_address = ++es_next_address;
        }
      }
      while (x_accept(","));
      
      x_expect("\n");
    }
    else if (x_accept("enum"))
    {
      i32 constant_value, si;
    
      x_accept("\n");
      x_expect("{");
      
      constant_value = 0;
      
      do
      {
        X_Token constant_name;
      
        x_accept("\n");
        constant_name = x_expect(" A");
        
        if (x_accept("="))
        {
          es_instruction_argument(&constant_value, NULL);
        }
        
        si = es_symbol_declare(ES_SYM_CONSTANT, ES_SYMF_DEFINED, constant_name);
        es_symtab[si].st_value.as_i64 = constant_value;
        
        ++constant_value;
        
        x_accept("\n");
      }
      while (x_accept(","));
      
      x_accept("\n");
      x_expect("}");
      x_accept(";");
      x_expect("\n");
    }
    else if (x_accept("const"))
    {
      i32 constant_value, si;
      
      do
      {
        si = es_symbol_declare(ES_SYM_CONSTANT, ES_SYMF_DEFINED, x_expect(" A"));
        x_expect("=");
        es_instruction_argument(&constant_value, NULL);
        es_symtab[si].st_value.as_i64 = constant_value;
      }
      while (x_accept(","));
      
      x_expect("\n");
    }
    else if (x_accept("call"))
    {
      X_Token token;
      
      if ((token = x_accept(" Q")))
      {
        char *data;
        int size;
        
        size = 0;
        data = x_decode(token, &size);
        
        fprintf(es_cmdout, "function %.*s\n", size, data);
      }
      else
      {
        token = x_expect(" A");
        es_symbol_declare(ES_SYM_LABEL, ES_SYMF_USED, token);
        fprintf(es_cmdout, "function %s%.*s\n",
          es_mcfunction_prefix, x_toklen(token), x_tokstr(token));
      }
      
      x_expect("\n");
      fflush(es_cmdout);
    }
    else if (x_accept("jmp"))
    {
      es_generate_jump(x_expect(" A"));
      x_expect("\n");
      es_labels_end_active_ones();
    }
    else if (x_accept("ret") || x_accept("unreachable"))
    {
      x_expect("\n");
      
      es_labels_end_active_ones();
    }
    else if (x_accept("xchg"))
    {
      i32 arg1_si, arg2_si;
      
      es_instruction_argument(NULL, &arg1_si);
      x_expect(",");
      es_instruction_argument(NULL, &arg2_si);
      x_expect("\n");
      
      fprintf(es_cmdout, "scoreboard players operation ");
      es_symbol_eternal_print_name(arg1_si);
      fprintf(es_cmdout, " %s >< ", es_objective_name);
      es_symbol_eternal_print_name(arg2_si);
      fprintf(es_cmdout, " %s\n", es_objective_name);
      
      fflush(es_cmdout);
    }
    else /* a label then? */
    {
      X_Token label_name;
      
      label_name = x_expect(" A");
      
      if (!x_accept(":"))
      {
        x_msgf(X_FERR, label_name, "no such instruction");
      }
      
      es_labels_define(label_name);
      
      if (es_add_comment_for_every_label)
      {
        fprintf(es_cmdout, "# %.*s\n", x_toklen(label_name), x_tokstr(label_name));
        fflush(es_cmdout);
      }
    }
  }
  
  /*************************************/
  /*-------/  SEMANTIC ERRORS  \-------*/
  /*************************************/
  
  for (i = 0; i < es_symtab_num; ++i)
  {
    if ((es_symtab[i].st_flags & ES_SYMF_USED) && !(es_symtab[i].st_flags & ES_SYMF_DEFINED))
    {
      x_msgf(X_FERR, es_symtab[i].st_name, "the symbol is used but never defiend");
    }
    
    if (!(es_symtab[i].st_flags & ES_SYMF_USED) && (es_symtab[i].st_flags & ES_SYMF_DEFINED)
     && !(es_symtab[i].st_flags & ES_SYMF_EXTERN))
    {
      x_msgf(X_WARN, es_symtab[i].st_name, "%s is unused", es_symbol_title[es_symtab[i].st_kind]);
    }
  }
  
  /***************************************/
  /*-------/  WRITE MCFUNCTIONS  \-------*/
  /***************************************/
  
  es_labels_end_active_ones();
  
  for (i = 0; i < es_labels_num; ++i)
  {
    static char path[PATH_MAX];
    
    FILE *stream;
    
    sprintf(path, "%s/%.*s.mcfunction", es_output_directory,
      x_toklen(es_labels[i].dl_name), x_tokstr(es_labels[i].dl_name));
    
    stream = fopen(path, "wb");
    
    if (stream == NULL)
    {
      fprintf(stderr, X_BOLD"%s: "X_BRED"error: "X_RESET"%s\n", path, strerror(errno));
      x_msgf(X_NOTE, es_labels[i].dl_name, "the corresponding label");
      exit(EXIT_FAILURE);
    }
    
    fwrite(es_cmdout_buffer + es_labels[i].dl_text_start, 1,
      es_labels[i].dl_text_end - es_labels[i].dl_text_start, stream);
    
    fclose(stream);
  }
  
  return 0;
}
/*************************************************************************************************/
i64 es_expression_evaluate(u32 ni)
{
  X_Token token;
  u32 kind;

  token = es_nodes[ni].n_token;
  kind = es_nodes[ni].n_kind;

  /**/ if (kind == ES_NODE_INTEGER)
  {
    return (i64)x_parse64(token);
  }
  else if (kind == ES_NODE_NAME)
  {
    i32 si;
    
    si = es_symbol_find(token);
    
    if (si < 0)
    {
      x_msgf(X_FERR, token, "undefined symbol");
    }
    
    if (es_symtab[si].st_kind != ES_SYM_CONSTANT)
    {
      x_msgf(X_FERR, token, "it is NOT a constant");
    }
    
    es_symtab[si].st_flags |= ES_SYMF_USED;
    
    return es_symtab[si].st_value.as_i64;
  }
  else if (kind == ES_NODE_CHARACTER)
  {
    char *data;
    int size;
    
    size = 0;
    data = x_decode(token, &size);
    
    if (size != 1)
    {
      x_msgf(X_FERR, token, "invalid character constant");
    }
    
    return data[0];
  }
  else if (kind == ES_NODE_PREFIX)
  {
    i64 x;
    
    x = es_expression_evaluate(es_nodes[ni].n_rhs);
    
    /**/ if (x_equals_mem(token, 1, "-")) x = -x;
    else if (x_equals_mem(token, 1, "~")) x = ~(u64)x;
    else if (x_equals_mem(token, 1, "!")) x = !x;
    else if (x_equals_mem(token, 1, "+")) x = (x < 0) ? -x : x;
    else x_msgf(X_FERR, token, "unimplemented feature");
    
    return x;
  }
  else if (kind == ES_NODE_BINARY && x_equals_mem(token, 1, "?"))
  {
    u32 colon;
    
    colon = es_nodes[ni].n_rhs;
    
    if (es_expression_evaluate(es_nodes[ni].n_lhs))
    {
      return es_expression_evaluate(es_nodes[colon].n_lhs);
    }
    else
    {
      return es_expression_evaluate(es_nodes[colon].n_rhs);
    }
  }
  else if (kind == ES_NODE_BINARY)
  {
    i64 lhv, rhv, y;
    
    lhv = es_expression_evaluate(es_nodes[ni].n_lhs);
    rhv = es_expression_evaluate(es_nodes[ni].n_rhs);
    
    /**/ if (x_equals_mem(token, 1, "+"))  y = lhv + rhv;
    else if (x_equals_mem(token, 1, "-"))  y = lhv - rhv;
    else if (x_equals_mem(token, 1, "*"))  y = lhv * rhv;
    else if (x_equals_mem(token, 1, "/"))  y = lhv / rhv;
    else if (x_equals_mem(token, 1, "%"))  y = lhv % rhv;
    else if (x_equals_mem(token, 1, "&"))  y = (u64)lhv & (u64)rhv;
    else if (x_equals_mem(token, 1, "|"))  y = (u64)lhv | (u64)rhv;
    else if (x_equals_mem(token, 1, "^"))  y = (u64)lhv ^ (u64)rhv;
    else if (x_equals_mem(token, 1, "<"))  y = lhv < rhv;
    else if (x_equals_mem(token, 1, ">"))  y = lhv > rhv;
    else if (x_equals_mem(token, 2, "<=")) y = lhv <= rhv;
    else if (x_equals_mem(token, 2, ">=")) y = lhv >= rhv;
    else if (x_equals_mem(token, 2, "==")) y = lhv == rhv;
    else if (x_equals_mem(token, 2, "!=")) y = lhv != rhv;
    else if (x_equals_mem(token, 2, "&&")) y = lhv && rhv;
    else if (x_equals_mem(token, 2, "||")) y = lhv || rhv;
    else if (x_equals_mem(token, 2, "<<")) y = lhv << rhv;
    else if (x_equals_mem(token, 2, ">>")) y = lhv >> rhv;
    else if (x_equals_mem(token, 2, "|<")) y = (lhv < rhv) ? lhv : rhv;
    else if (x_equals_mem(token, 2, "|>")) y = (lhv > rhv) ? lhv : rhv;
    else x_msgf(X_FERR, token, "unimplemented feature");
    
    return y;
  }
  else
  {
    x_msgf(X_FERR, token, "unimplemented feature");
  }
  
  return 0;
}
/*************************************************************************************************/
u32 es_expression_parse_bp(u32 min_bp)
{
#define ATOM(selector, kind)    {(selector), (kind),      (kind)}
#define PREF(selector)          {(selector), 100,         100}
#define LEFT(selector, bp_base) {(selector), (bp_base)-1, (bp_base)+1}
  static const Operator atmtab[] =
  { ATOM(" A", ES_NODE_NAME), ATOM(" D", ES_NODE_INTEGER), ATOM(" \'", ES_NODE_CHARACTER) };
  static const Operator pretab[] = { PREF("!"), PREF("~"), PREF("-"), PREF("+") };
  static const Operator bintab[] = {
    {"?", 11, 9},
    LEFT("||", 15), LEFT("&&", 20),
    LEFT("<", 25),  LEFT("<=", 25), LEFT(">", 25), LEFT(">=", 25), LEFT("==", 25), LEFT("!=", 25),
    LEFT("|<", 30), LEFT("|>", 30),
    LEFT("|", 35),  LEFT("&", 40),  LEFT("^", 45),
    LEFT("<<", 50), LEFT(">>", 50),
    LEFT("+", 55),  LEFT("-", 55),
    LEFT("*", 60),  LEFT("/", 60),  LEFT("%", 60)
  };

  const Operator *op;
  X_Token token, mtoken;
  u32 lhs, rhs, mhs;
  
  /**/ if ((op = es_operator_find(atmtab, numof(atmtab))) != NULL)
  {
    lhs = es_nodes_create_one(x_read(), op->o_lbp, 0, 0);
  }
  else if ((op = es_operator_find(pretab, numof(pretab))) != NULL)
  {
    token = x_read();
    rhs = es_expression_parse_bp(op->o_rbp);
    lhs = es_nodes_create_one(token, ES_NODE_PREFIX, 0, rhs);
  }
  else
  {
    x_expect("(");
    lhs = es_expression_parse_bp(0);
    x_expect(")");
  }
  
  for (;;)
  {
    if ((op = es_operator_find(bintab, numof(bintab))) != NULL)
    {
      if (op->o_lbp < min_bp)
      {
        break;
      }
      
      token = x_read();
      
      if (x_equals_mem(token, 1, "?"))
      {
        mhs = es_expression_parse_bp(0);
        mtoken = x_expect(":");
        rhs = es_expression_parse_bp(op->o_rbp);
        rhs = es_nodes_create_one(mtoken, ES_NODE_BINARY, mhs, rhs);
        lhs = es_nodes_create_one(token, ES_NODE_BINARY, lhs, rhs);
      }
      else
      {
        rhs = es_expression_parse_bp(op->o_rbp);
        lhs = es_nodes_create_one(token, ES_NODE_BINARY, lhs, rhs);
      }
      
      continue;
    }
    
    break;
  }
  
  return lhs;
}
/*************************************************************************************************/
void es_generate_branch(u32 type)
{
  static const char *opcode_table[ES_JCOUNT] = { "<", "<=", "=", "=", ">", ">=" };
  static const char *altsubcmds[ES_JCOUNT] = { "unless", "if", "if", "unless", "unless", "if" };
  static const char *altformats[ES_JCOUNT] = { "%i..", "..%i", "%i", "%i", "..%i", "%i.." };
  
  const char *subcmd, *opcode;
  X_Token label_name;
  i32 arg1_i32, arg1_si, arg2_i32, arg2_si;
  
  subcmd = (type == ES_JNE) ? "unless" : "if";
  opcode = opcode_table[type];
  
  label_name = x_expect(" A");
  x_expect(",");
  es_instruction_argument(&arg1_i32, &arg1_si);
  x_expect(",");
  es_instruction_argument(&arg2_i32, &arg2_si);
  x_expect("\n");
  
  if (arg1_si < 0 && arg2_si < 0)
  {
    if ((type == ES_JLE && arg1_i32 <= arg2_i32)
     || (type == ES_JL  && arg1_i32 <  arg2_i32)
     || (type == ES_JGE && arg1_i32 >= arg2_i32)
     || (type == ES_JG  && arg1_i32 >  arg2_i32)
     || (type == ES_JE  && arg1_i32 == arg2_i32)
     || (type == ES_JNE && arg1_i32 != arg2_i32))
    {
      es_generate_jump(label_name);
      es_labels_end_active_ones();
    }
    
    return;
  }

  if (arg1_si >= 0 && arg2_si >= 0)
  {
    fprintf(es_cmdout, "execute %s score ", subcmd);
    es_symbol_eternal_print_name(arg1_si);
    fprintf(es_cmdout, " %s %s ", es_objective_name, opcode);
    es_symbol_eternal_print_name(arg2_si);
    fprintf(es_cmdout, " %s run ", es_objective_name);
    es_generate_jump(label_name);
    
    return;
  }

  if (arg1_si < 0) /* swap so arg1 is eternal and arg2 is constant */
  {
    arg1_si = arg2_si;
    arg2_i32 = arg1_i32;
  }
  
  fprintf(es_cmdout, "execute %s score ", altsubcmds[type]);
  es_symbol_eternal_print_name(arg1_si);
  fprintf(es_cmdout, " %s matches ", es_objective_name);
  fprintf(es_cmdout, altformats[type], arg2_i32);
  fprintf(es_cmdout, " run ");
  es_generate_jump(label_name);
}
/*************************************************************************************************/
void es_generate_jump(X_Token label_name)
{
  es_symbol_declare(ES_SYM_LABEL, ES_SYMF_USED, label_name);
  fprintf(es_cmdout, "return run function %s%.*s\n",
    es_mcfunction_prefix, x_toklen(label_name), x_tokstr(label_name));
  fflush(es_cmdout);
}
/*************************************************************************************************/
void es_generate_operation(const char *opcode, const char *altcode, i32 stupid_bug)
{
  i32 arg1_si, arg2_i32, arg2_si;

  es_instruction_argument(NULL, &arg1_si);
  x_expect(",");
  es_instruction_argument(&arg2_i32, &arg2_si);
  x_expect("\n");

  if (arg2_si < 0)
  {
    if (altcode != NULL && !(stupid_bug && (u32)arg2_i32 == 0x80000000))
    {
      fprintf(es_cmdout, "scoreboard players %s ", altcode);
      es_symbol_eternal_print_name(arg1_si);
      fprintf(es_cmdout, " %s %i\n", es_objective_name, arg2_i32);
      
      fflush(es_cmdout);
      return;
    }
  
    fprintf(es_cmdout, "scoreboard players set %s0000 %s %i\n", es_eternal_prefix,
      es_objective_name, arg2_i32);
    arg2_si = 0;
  }

  fprintf(es_cmdout, "scoreboard players operation ");
  es_symbol_eternal_print_name(arg1_si);
  fprintf(es_cmdout, " %s %s ", es_objective_name, opcode);
  es_symbol_eternal_print_name(arg2_si);
  fprintf(es_cmdout, " %s\n", es_objective_name);
  
  fflush(es_cmdout);
}
/*************************************************************************************************/
void es_instruction_argument(i32 *constant_out, i32 *si_out)
{
  u32 ni;
  i32 si;
  
  if (si_out != NULL)       *si_out = -1;
  if (constant_out != NULL) *constant_out = 0;

  es_nodes_top = 0;
  ni = es_expression_parse_bp(0);
  si = -1;
  
  if (es_nodes[ni].n_kind == ES_NODE_NAME && (si = es_symbol_find(es_nodes[ni].n_token)) < 0)
  {
    x_msgf(X_FERR, es_nodes[ni].n_token, "undefined symbol");
  }
  
  if (si >= 0)
  {
    if (es_symtab[si].st_kind == ES_SYM_ETERNAL)
    {
      if (si_out == NULL)
      {
        x_msgf(X_FERR, es_nodes[ni].n_token, "only a constexpr is allowed here");
      }
      
      es_symtab[si].st_flags |= ES_SYMF_USED;
      *si_out = si;
      return;
    }
    else if (constant_out == NULL)
    {
      x_msgf(X_ERR, es_nodes[ni].n_token, "it is NOT an eternal variable");
      x_msgf(X_NOTE, es_symtab[si].st_name, "previously declared as %s",
        es_symbol_title[es_symtab[si].st_kind]);
      exit(EXIT_FAILURE);
    }
  }

  if (constant_out == NULL)
  {
    x_msgf(X_FERR, es_nodes[ni].n_token, "only an eternal variable is allowed here");
  }

  *constant_out = (i32)es_expression_evaluate(ni);
}
/*************************************************************************************************/
i32 es_labels_define(X_Token name)
{
  Es_Defined_Label label;
  i32 si;
  
  si = es_symbol_find(name);
  
  if (si >= 0 && (es_symtab[si].st_flags & ES_SYMF_DEFINED) != 0)
  {
    x_msgf(X_ERR, name, "label redefinition");
    x_msgf(X_NOTE, es_symtab[si].st_name, "previous definition");
    exit(EXIT_FAILURE);
  }
  
  si = es_symbol_declare(ES_SYM_LABEL, ES_SYMF_DEFINED, name);
  es_symtab[si].st_value.as_index = es_labels_num;
  es_symtab[si].st_name = name;

  label.dl_name = name;
  label.dl_text_start = es_cmdout_size;
  label.dl_text_end = 0;
  
  fwrite(&label, 1, sizeof(label), es_labels_stream);
  fflush(es_labels_stream);
  
  return es_symtab[si].st_value.as_index;
}
/*************************************************************************************************/
void es_labels_end_active_ones(void)
{
  u32 i;

  for (i = es_labels_active_start; i < es_labels_num; ++i)
  {
    es_labels[i].dl_text_end = es_cmdout_size;
  }
  
  es_labels_active_start = es_labels_num;
}
/*************************************************************************************************/
u32 es_nodes_create_one(X_Token token, u8 kind, u8 lhs, u8 rhs)
{
  ++es_nodes_top;
  
  if (es_nodes_top >= numof(es_nodes))
  {
    x_msgf(X_FERR, token, "out of memory");
  }
  
  es_nodes[es_nodes_top].n_token = token;
  es_nodes[es_nodes_top].n_kind = kind;
  es_nodes[es_nodes_top].n_lhs = lhs;
  es_nodes[es_nodes_top].n_rhs = rhs;
  
  return es_nodes_top;
}
/*************************************************************************************************/
const Operator *es_operator_find(const Operator *tab, u32 num)
{
  u32 i;
  
  for (i = 0; i < num; ++i)
  {
    if (x_verify(tab[i].o_selector))
    {
      return &tab[i];
    }
  }
  
  return NULL;
}
/*************************************************************************************************/
i32 es_symbol_declare(u16 kind, u16 flags, X_Token name)
{
  Es_Symbol symbol;
  i32 si;
  
  si = es_symbol_find(name);
  
  if (si >= 0)
  {
    if (es_symtab[si].st_kind != kind)
    {
      x_msgf(X_ERR, name, "it is NOT %s", es_symbol_title[kind]);
      x_msgf(X_NOTE, es_symtab[si].st_name, "previously declared as %s",
        es_symbol_title[es_symtab[si].st_kind]);
      exit(EXIT_FAILURE);
    }
    
    es_symtab[si].st_flags |= flags;
    
    return si;
  }
  
  symbol.st_name = name;
  symbol.st_flags = flags;
  symbol.st_kind = kind;
  
  fwrite(&symbol, 1, sizeof(symbol), es_symtab_stream);
  fflush(es_symtab_stream);
  
  return es_symtab_num - 1;
}
/*************************************************************************************************/
void es_symbol_eternal_print_name(i32 si)
{
  if (es_symtab[si].st_flags & ES_SYMF_EXTERN)
  {
    fprintf(es_cmdout, "%.*s", x_toklen(es_symtab[si].st_name), x_tokstr(es_symtab[si].st_name));
  }
  else
  {
    fprintf(es_cmdout, "%s%04X", es_eternal_prefix, es_symtab[si].st_value.as_address);
  }
}
/*************************************************************************************************/
i32 es_symbol_find(X_Token name)
{
  u32 i;
  
  for (i = 0; i < es_symtab_num; ++i)
  {
    if (x_equals(es_symtab[i].st_name, name))
    {
      return i;
    }
  }
  
  return -1;
}
/*************************************************************************************************/
FILE *xopen_memstream(char **ptr, size_t *sizeloc)
{
  FILE *stream;
  
  stream = open_memstream(ptr, sizeloc);
  
  if (stream == NULL)
  {
    x_msgf(X_FERR, 0, "%s", strerror(errno));
  }
  
  return stream;
}
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

