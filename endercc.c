/* endercc.c - EnderC Compiler
LICENSE: See end of file for license information.
BUILD: $ cc -std=c89 -pedantic -pedantic-errors -o endercc endercc.c
USAGE: $ endercc [options] <file.endc>
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
#define X_MODE_ENDER_C
#define LUXUR_C
#include "luxur.h"
/*************************************************************************************************/
#define LABEL_FORMAT "label_%04i"
/*************************************************************************************************/
#define numof(array) (sizeof(array)/sizeof((array)[0]))
/*************************************************************************************************/
typedef signed char   i8; typedef signed short   i16; typedef signed int   i32;
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
/*************************************************************************************************/
typedef unsigned long u64;
typedef signed long   i64;
/*************************************************************************************************/
typedef struct Ec_Operator {
  const char *o_selector;
  u16 o_lbp;
  u16 o_rbp;
  u32 o_type;
} Ec_Operator;
/*************************************************************************************************/
enum /* Ec_Node_Type */ {
  ECN_NULL, ECN_NAME, ECN_INTEGER, ECN_CHARACTER, ECN_STRING, ECN_TERNARY,
  ECN_BEG_BINARY,
    ECN_COMMA, ECN_CALL, ECN_AT,
    ECN_BEG_MATH,
      ECN_ADD, ECN_SUB, ECN_MUL, ECN_DIV, ECN_MOD, ECN_MIN, ECN_MAX,
    ECN_END_MATH,
    ECN_BEG_CONSTEXPR,
      ECN_AND, ECN_OR, ECN_XOR, ECN_SHL, ECN_SHR,
    ECN_END_CONSTEXPR,
    ECN_BEG_ASSING,
      ECN_ASSIGN, ECN_ADDS, ECN_SUBS, ECN_MULS, ECN_DIVS, ECN_MODS, ECN_MINS, ECN_MAXS, ECN_XCHG,
    ECN_END_ASSING,
  ECN_END_BINARY,
  ECN_BEG_SUFFIX,
    ECN_SUFINC, ECN_SUFDEC,
  ECN_END_SUFFIX,
  ECN_BEG_PREFIX,
    ECN_NEG, ECN_ABS, ECN_NOT, ECN_PREINC, ECN_PREDEC,
  ECN_END_PREFIX,
  ECN_BEG_LOGIC,
    ECN_LNOT, ECN_LOR, ECN_LAND, ECN_LT, ECN_LE, ECN_EQ, ECN_NE, ECN_GT, ECN_GE,
  ECN_END_LOGIC
};
/*************************************************************************************************/
typedef struct Ec_Node {
  X_Token n_token;
  u8 n_lhs;
  u8 n_rhs;
  u8 n_type;
} Ec_Node;
/*************************************************************************************************/
enum { EC_DECL_GLOBAL, EC_DECL_LOCAL, EC_DECL_PARAM };
/*************************************************************************************************/
enum { EC_VALUE_VOID, EC_VALUE_CONSTANT, EC_VALUE_REGISTER, EC_VALUE_VARIABLE, EC_VALUE_FUNCTION,
  EC_VALUE_ADDRESS };
/*************************************************************************************************/
typedef struct Ec_Value {
  u32 v_kind;
  union {
    i64 as_i64;
    u32 as_regi;
    u32 as_symi;
    u32 as_address;
  } v_data;
} Ec_Value;
/*************************************************************************************************/
enum { EC_SYM_CONSTANT, EC_SYM_VARIABLE, EC_SYM_FUNCTION };
/*************************************************************************************************/
enum { EC_FLG_DEFINED = 1, EC_FLG_USED = 2, EC_FLG_EXTERN = 4 };
/*************************************************************************************************/
typedef struct Ec_Symbol {
  X_Token st_name;
  u32 st_scope;
  u16 st_kind;
  u16 st_flags;
  union {
    i64 as_i64;
    u32 as_address;
    struct {
      u32 f_addrbase;
      u8 f_argnum;
      u8 f_retval;
      u8 f_signatured;
    } as_func;
  } st_value;
} Ec_Symbol;
/*************************************************************************************************/
enum { EC_BRANCH_JL, EC_BRANCH_JLE, EC_BRANCH_JE, EC_BRANCH_JNE, EC_BRANCH_JG, EC_BRANCH_JGE,
  EC_BRANCH_COUNT };
/*************************************************************************************************/
typedef struct Ec_Label {
  X_Token l_name;
  u32 l_id;
  u32 l_is_defined;
} Ec_Label;
/*************************************************************************************************/
#define ec_branch_reverse(br) (EC_BRANCH_COUNT - 1 - (br))
/*************************************************************************************************/
static u32  ec_compile_declaration(u32 type);
static Ec_Value ec_compile_expression(u32 ni);
static Ec_Value ec_compile_function_call(Ec_Node n);
static void ec_compile_logical_and_item(u32 ni, u32 notitem, u32 label);
static void ec_compile_logical_and_list(u32 ni, u32 notlist, u32 notitem, u32 label, u32 is_or);
static u32  ec_compile_statement(u32 break_label, u32 continue_label);
static i64  ec_const_expression_evaluate(u32 ni);
static u32  ec_expression_parse_bp(u32 min_bp);
static Ec_Label *ec_local_label_declare(X_Token name);
static void ec_local_label_define(X_Token name);
static u32  ec_nodes_create_one(X_Token token, u8 type, u8 lhs, u8 rhs);
static u32  ec_nodes_unnot(u32 ni, u32 *is_not_out);
static const Ec_Operator *ec_operator_find(const Ec_Operator *tab, u32 num);
static i32  ec_symbols_find(X_Token name);
static u32  ec_symbols_find_you_must(X_Token name);
static u32  ec_symbols_declare(X_Token name, u16 kind, u16 flags);
static Ec_Value ec_value_allocate_register(void);
static void ec_value_free_register(Ec_Value value);
static Ec_Value ec_value_from_symbol(u32 si);
static void ec_value_print(Ec_Value value);
static Ec_Value ec_value_to_register(Ec_Value value);
static Ec_Value ec_value_to_the_register(Ec_Value value, Ec_Value inreg);
/*************************************************************************************************/
static FILE *ec_asmout;
/*************************************************************************************************/
static Ec_Node ec_nodes[1024];
static u32     ec_nodes_top;
/*************************************************************************************************/
static Ec_Symbol ec_symtab[1024];
static i32       ec_symtab_top;
/*************************************************************************************************/
static u32 ec_next_address;
static u32 ec_next_label_index;
/*************************************************************************************************/
static u64 ec_regs_taken;
static u64 ec_regs_declared;
/*************************************************************************************************/
static Ec_Label ec_local_labels[128];
static u32 ec_local_labels_num;
/*************************************************************************************************/
static Ec_Value ec_current_function_return_address;
/*************************************************************************************************/
static u32 ec_current_scope;
/*************************************************************************************************/
i32 main(i32 argc, char *argv[])
{
  i32 k;
  char *source_path, *output_path;

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
           "  -o <file>                Or\n"
           "  --output <file>          Place the output into <file>.\n"
           "\n");
    exit(EXIT_SUCCESS);
  }
  
  if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
  {
    printf("endercc 1.0.5\n"
           "This is free and unencumbered software released into the public domain.\n"
           "For more information, please refer to <https://unlicense.org/>\n\n");
    exit(EXIT_SUCCESS);
  }
  
  source_path = NULL;
  output_path = "./a.s";
  
  for (k = 1; k < argc; ++k)
  {
    /**/ if (strcmp(argv[k], "-o") == 0 || strcmp(argv[k], "--output") == 0)
    {
      ++k;
      
      if (k >= argc)
      {
        x_msgf(X_FERR, 0, "invalid '%s' option", argv[k - 1]);
      }
      
      output_path = argv[k];
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
  
  x_init(strlen(source_path), source_path);
  
  ec_asmout = fopen(output_path, "wb");
  
  if (ec_asmout == NULL)
  {
    fprintf(stderr, X_BOLD"%s: "X_BRED"error: "X_RESET"%s\n", output_path, strerror(errno));
    exit(EXIT_FAILURE);
  }
  
  /************************************/
  /*-------/  THE GREAT LOOP  \-------*/
  /************************************/
  
  while (ec_compile_declaration(EC_DECL_GLOBAL))
  {
  }
  
  return 0;
}
/*************************************************************************************************/
u32 ec_compile_declaration(u32 type)
{
  X_Token name;
  i32 is_eternal, is_constant, is_function_definition;
  u32 flags;
  
  if (x_accept("enum"))
  {
    i64 value;
    u32 si, ni, ci, ntop, last_constant;
    
    value = 0;
    
    x_expect("{");
    
    ntop = ec_nodes_top;
    ni = ec_expression_parse_bp(0);
    
    for (last_constant = 0; !last_constant; )
    {
      if (ec_nodes[ni].n_type == ECN_COMMA)
      {
        ci = ec_nodes[ni].n_lhs;
        ni = ec_nodes[ni].n_rhs;
      }
      else
      {
        ci = ni;
        last_constant = 1;
      }
      
      if (ec_nodes[ci].n_type == ECN_NAME)
      {
        name = ec_nodes[ci].n_token;
      }
      else if (ec_nodes[ci].n_type == ECN_ASSIGN)
      {
        value = ec_const_expression_evaluate(ec_nodes[ci].n_rhs);
        ci = ec_nodes[ci].n_lhs;
        name = ec_nodes[ci].n_token;
        
        if (ec_nodes[ci].n_type != ECN_NAME)
        {
          x_msgf(X_FERR, ec_nodes[ci].n_token, "not a name");
        }
      }
      else
      {
        x_msgf(X_FERR, ec_nodes[ci].n_token, "unexpected token");
      }
      
      si = ec_symbols_declare(name, EC_SYM_CONSTANT, EC_FLG_DEFINED);
      ec_symtab[si].st_value.as_i64 = value;
      ++value;
    }

    ec_nodes_top = ntop;
    
    x_expect("}");
    x_accept(";");
    
    return 1;
  }
  
  flags = 0;
  is_constant = 0;
  
  if (x_accept("extern"))
  {
    flags |= EC_FLG_EXTERN;
  }
  else if (x_accept("const"))
  {
    is_constant = 1;
  }
  
  /**/ if (x_accept("eter"))
  {
    is_eternal = 1;
  }
  else if (x_accept("void"))
  {
    is_eternal = 0;
  }
  else if (flags || is_constant)
  {
    x_expect("eter");
  }
  else
  {
    return 0;
  }
  
  do
  {
    is_function_definition = 0;
    name = x_expect(" A");
    
    if (type != EC_DECL_PARAM && !is_constant && x_accept("("))
    {
      X_Token arg_name;
      u32 li, fi, argnum, top, addr;
      
      fi = ec_symbols_declare(name, EC_SYM_FUNCTION, flags);
      
      if (ec_symtab[fi].st_value.as_func.f_signatured == 0)
      {
        Ec_Value retaddr;
      
        addr = ++ec_next_address;
        ec_symtab[fi].st_value.as_func.f_addrbase = addr;
        ec_symtab[fi].st_value.as_func.f_retval = is_eternal;
        
        if (is_eternal)
        {
          retaddr.v_kind = EC_VALUE_ADDRESS;
          retaddr.v_data.as_address = addr;
          
          fprintf(ec_asmout, "eter ");
          ec_value_print(retaddr);
          fprintf(ec_asmout, "\n");
        }
      }
      else
      {
        addr = ec_symtab[fi].st_value.as_func.f_addrbase;
      }
      
      ++ec_current_scope;
      top = ec_symtab_top;

      argnum = 0;
      
      if (!x_verify(")"))
      {
        do
        {
          u32 si_arg;
          
          x_expect("eter");
          arg_name = x_expect(" A");
          si_arg = ec_symbols_declare(arg_name, EC_SYM_VARIABLE, EC_FLG_DEFINED);
          ec_symtab[si_arg].st_value.as_address = ++addr;
          ++argnum;
          
          if (ec_symtab[fi].st_value.as_func.f_signatured == 0)
          {
            fprintf(ec_asmout, "eter ");
            ec_value_print(ec_value_from_symbol(si_arg));
            fprintf(ec_asmout, "\n");
          }
        }
        while (x_accept(","));
      }
      
      x_expect(")");

      if (ec_symtab[fi].st_value.as_func.f_signatured == 0)
      {
        ec_symtab[fi].st_value.as_func.f_argnum = argnum;
        ec_next_address = addr;
        ec_symtab[fi].st_value.as_func.f_signatured = 1;
      }
      else if (ec_symtab[fi].st_value.as_func.f_argnum != argnum
            || ec_symtab[fi].st_value.as_func.f_retval != is_eternal)
      {
        x_msgf(X_ERR, name, "the signature of the function is different");
        x_msgf(X_NOTE, ec_symtab[fi].st_name, "previous declaration");
        exit(EXIT_FAILURE);
      }

      if (flags & EC_FLG_EXTERN)
      {
        fprintf(ec_asmout, "extern func %.*s\n", x_toklen(name), x_tokstr(name));
      }
      
      if (type == EC_DECL_GLOBAL && x_verify("{"))
      {
        fprintf(ec_asmout, "%.*s:\n", x_toklen(name), x_tokstr(name));
      
        ec_symtab[fi].st_flags |= EC_FLG_DEFINED;
        ec_current_function_return_address.v_kind = EC_VALUE_ADDRESS;
        ec_current_function_return_address.v_data.as_address =
                                                         ec_symtab[fi].st_value.as_func.f_addrbase;
      
        if (!ec_compile_statement(0, 0))
        {
          fprintf(ec_asmout, "  ret\n");
        }
        
        for (li = 0; li < ec_local_labels_num; ++li)
        {
          if (ec_local_labels[li].l_is_defined == 0)
          {
            x_msgf(X_FERR, ec_local_labels[li].l_name, "the label is used but never defined");
          }
        }
        
        is_function_definition = 1;
      }
      
      ec_symtab_top = top;
      --ec_current_scope;
    }
    else if (is_constant)
    {
      i64 value;
      u32 si, ni, ntop;
      
      x_expect("=");
      
      ntop = ec_nodes_top;
      ni = ec_expression_parse_bp(0);
      value = ec_const_expression_evaluate(ni);
      ec_nodes_top = ntop;
    
      si = ec_symbols_declare(name, EC_SYM_CONSTANT, EC_FLG_DEFINED);
      ec_symtab[si].st_value.as_i64 = value;
    }
    else if (is_eternal)
    {
      Ec_Value value;
      u32 si;
    
      si = ec_symbols_declare(name, EC_SYM_VARIABLE, flags | EC_FLG_DEFINED);
      ec_symtab[si].st_value.as_address = ++ec_next_address;
      value = ec_value_from_symbol(si);
      
      if (flags & EC_FLG_EXTERN)
      {
        fprintf(ec_asmout, "extern ");
      }
      
      fprintf(ec_asmout, "eter ");
      ec_value_print(value);
      fprintf(ec_asmout, "\n");
    }
    else
    {
      x_msgf(X_FERR, name, "a void variable");
    }
  }
  while (x_accept(","));
  
  if (!is_function_definition)
  {
    x_expect(";");
  }
  
  return 1;
}
/*************************************************************************************************/
Ec_Value ec_compile_expression(u32 ni)
{
  Ec_Value result;
  Ec_Node n;
  
  memset(&result, 0, sizeof(result));
  
  n = ec_nodes[ni];
  
  /**/ if (n.n_type == ECN_CALL)
  {
    if (ec_nodes[n.n_lhs].n_type != ECN_NAME)
    {
      x_msgf(X_FERR, ec_nodes[n.n_lhs].n_token, "it is NOT a function");
    }
    
    if (x_equals_mem(ec_nodes[n.n_lhs].n_token, 7, "tellraw")
     || x_equals_mem(ec_nodes[n.n_lhs].n_token, 3, "cmd"))
    {
      u32 is_tellraw;
      Ec_Node arg, list;
      u32 num_args, the_last_one, ni_arg, ni_list;
    
      is_tellraw = x_equals_mem(ec_nodes[n.n_lhs].n_token, 7, "tellraw");
    
      fprintf(ec_asmout, "  %s ", is_tellraw ? "tellraw" : "cmd");
      
      ni_list = n.n_rhs;
      arg = list = ec_nodes[ni_list];
      
      the_last_one = 0;
      num_args = 0;
      
      for (;;)
      {
        if (x_tokstr(list.n_token)[0] == ',')
        {
          ni_arg = list.n_lhs;
          arg = ec_nodes[ni_arg];
          ni_list = list.n_rhs;
          list = ec_nodes[ni_list];
        }
        else
        {
          arg = list;
          ni_arg = ni_list;
          the_last_one = 1;
        }
        
        /**/ if (arg.n_type == ECN_STRING)
        {
          fprintf(ec_asmout, "%.*s", x_toklen(arg.n_token), x_tokstr(arg.n_token));
        }
        else if (arg.n_type == ECN_NAME)
        {
          Ec_Value value;
          
          value = ec_value_from_symbol(ec_symbols_find_you_must(arg.n_token));
          ec_value_print(value);
        }
        else if (!is_tellraw && arg.n_type == ECN_CALL
         && x_equals_mem(ec_nodes[arg.n_lhs].n_token, 3, "nbt"))
        {
          Ec_Value value;
          
          if (ec_nodes[arg.n_rhs].n_type != ECN_NAME)
          {
            x_msgf(X_FERR, ec_nodes[arg.n_rhs].n_token, "it is NOT a name");
          }
          
          value = ec_value_from_symbol(ec_symbols_find_you_must(ec_nodes[arg.n_rhs].n_token));
          
          fprintf(ec_asmout, "tellraw ");
          ec_value_print(value);
        }
        else
        {
          i64 const_value;
          
          const_value = ec_const_expression_evaluate(ni_arg);
        
          fprintf(ec_asmout, "%li", const_value);
        }
        
        ++num_args;
        
        if (the_last_one)
        {
          break;
        }
        
        fprintf(ec_asmout, ", ");
      }
      
      fprintf(ec_asmout, "\n");
      
      if (is_tellraw && num_args < 2)
      {
        x_msgf(X_FERR, ec_nodes[n.n_lhs].n_token,
          "tellraw requires a selector and something to print");
      }
      
      return result; /* void */
    }
    else
    {
      return ec_compile_function_call(n);
    }
  }
  else if (ECN_BEG_ASSING < n.n_type && n.n_type < ECN_END_ASSING)
  {
    static const char *opcodes[] = { "mov","add","sub","mul","div","mod","min","max","xchg" };
    
    Ec_Value lvalue, rvalue;
    u32 subtype;
    
    subtype = n.n_type - ECN_BEG_ASSING - 1;
    
    lvalue = ec_compile_expression(n.n_lhs);
    
    if (lvalue.v_kind != EC_VALUE_VARIABLE)
    {
      x_msgf(X_FERR, ec_nodes[n.n_lhs].n_token, "it is NOT an lvalue");
    }
    
    rvalue = ec_compile_expression(n.n_rhs);
    
    if (n.n_type == ECN_XCHG && rvalue.v_kind != EC_VALUE_VARIABLE)
    {
      x_msgf(X_FERR, ec_nodes[n.n_rhs].n_token, "it is NOT an lvalue");
    }
    
    fprintf(ec_asmout, "  %s ", opcodes[subtype]);
    ec_value_print(lvalue);
    fprintf(ec_asmout, ", ");
    ec_value_print(rvalue);
    fprintf(ec_asmout, "\n");
    
    ec_value_free_register(rvalue);
    
    return lvalue;
  }
  else if (ECN_BEG_MATH < n.n_type && n.n_type < ECN_END_CONSTEXPR)
  {
    static const char *opcodes[] = { "add","sub","mul","div","mod","min","max" };
  
    Ec_Value lvalue, rvalue;
    u32 subtype;
    
    subtype = n.n_type - ECN_BEG_MATH - 1;
    
    lvalue = ec_compile_expression(n.n_lhs);
    rvalue = ec_compile_expression(n.n_rhs);
    
    if (lvalue.v_kind == EC_VALUE_CONSTANT && rvalue.v_kind == EC_VALUE_CONSTANT)
    {
      i64 y, x;
      
      y = lvalue.v_data.as_i64;
      x = rvalue.v_data.as_i64;
      
      if ((n.n_type == ECN_DIV || n.n_type == ECN_MOD) && x == 0)
      {
        x_msgf(X_FERR, n.n_token, "division by zero");
      }
      
      switch (n.n_type)
      {
      case ECN_ADD: y += x;  break;
      case ECN_SUB: y -= x;  break;
      case ECN_MUL: y *= x;  break;
      case ECN_DIV: y /= x;  break;
      case ECN_MOD: y %= x;  break;
      case ECN_SHL: y <<= x; break;
      case ECN_SHR: y >>= x; break;
      case ECN_AND: y &= x;  break;
      case ECN_OR:  y |= x;  break;
      case ECN_XOR: y ^= x;  break;
      }
    
      result.v_kind = EC_VALUE_CONSTANT;
      result.v_data.as_i64 = y;
      return result;
    }
    
    if (n.n_type > ECN_BEG_CONSTEXPR)
    {
      x_msgf(X_FERR, n.n_token, "this operation is available only for constants");
    }
    
    result = ec_value_to_register(lvalue);
    
    fprintf(ec_asmout, "  %s ", opcodes[subtype]);
    ec_value_print(result);
    fprintf(ec_asmout, ", ");
    ec_value_print(rvalue);
    fprintf(ec_asmout, "\n");
    
    ec_value_free_register(rvalue);
  }
  else if (n.n_type == ECN_NAME)
  {
    u32 si;
    
    si = ec_symbols_find_you_must(n.n_token);
    
    if (ec_symtab[si].st_kind == EC_SYM_FUNCTION)
    {
      x_msgf(X_FERR, n.n_token, "function is used like a varialbe");
    }
  
    result = ec_value_from_symbol(si);
  }
  else if (n.n_type == ECN_INTEGER)
  {
    result.v_kind = EC_VALUE_CONSTANT;
    result.v_data.as_i64 = x_parse64(n.n_token);
  }
  else if (n.n_type == ECN_CHARACTER)
  {
    i32 size;
    char *data;
    
    size = 0;
    data = x_decode(n.n_token, &size);
    
    if (size != 0)
    {
      x_msgf(X_FERR, n.n_token, "invalid character constant");
    }
  
    result.v_kind = EC_VALUE_CONSTANT;
    result.v_data.as_i64 = data[0];
  }
  else if (ECN_BEG_LOGIC < n.n_type && n.n_type < ECN_END_LOGIC)
  {
    Ec_Value zero;
    u32 notlist, zero_label;
    
    zero_label = ++ec_next_label_index;
    
    zero.v_kind = EC_VALUE_CONSTANT;
    zero.v_data.as_i64 = 0;
    result = ec_value_to_register(zero);
    
    ni = ec_nodes_unnot(ni, &notlist);
    ec_compile_logical_and_list(ni, notlist, 0, zero_label, 0);
    fprintf(ec_asmout, "  mov ");
    ec_value_print(result);
    fprintf(ec_asmout, ", 1\n");
    fprintf(ec_asmout, LABEL_FORMAT":\n", zero_label);
  }
  else if (n.n_type == ECN_TERNARY)
  {
    Ec_Value mv, rv;
    u32 li, mi, ri, else_label, quit_label, notlist;
    
    else_label = ++ec_next_label_index;
    quit_label = ++ec_next_label_index;
    
    li = n.n_lhs;
    mi = ec_nodes[n.n_rhs].n_lhs;
    ri = ec_nodes[n.n_rhs].n_rhs;
    
    result = ec_value_allocate_register();
    
    li = ec_nodes_unnot(li, &notlist);
    ec_compile_logical_and_list(li, notlist, 0, else_label, 0);
    
    mv = ec_compile_expression(mi);
    ec_value_to_the_register(mv, result);

    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", quit_label);
    fprintf(ec_asmout, LABEL_FORMAT":\n", else_label);
    
    rv = ec_compile_expression(ri);
    ec_value_to_the_register(rv, result);
    
    fprintf(ec_asmout, LABEL_FORMAT":\n", quit_label);
  }
  else if (n.n_type == ECN_PREDEC || n.n_type == ECN_PREINC)
  {
    Ec_Value rvalue;
    
    rvalue = ec_compile_expression(n.n_rhs);
    
    if (rvalue.v_kind != EC_VALUE_VARIABLE)
    {
      x_msgf(X_FERR, ec_nodes[n.n_rhs].n_token, "it is NOT an lvalue");
    }
    
    fprintf(ec_asmout, "  %s ", n.n_type == ECN_PREDEC ? "dec" : "inc");
    ec_value_print(rvalue);
    fprintf(ec_asmout, "\n");
    
    return rvalue;
  }
  else if (n.n_type == ECN_SUFDEC || n.n_type == ECN_SUFINC)
  {
    Ec_Value lvalue, reg;
    
    lvalue = ec_compile_expression(n.n_lhs);
    
    if (lvalue.v_kind != EC_VALUE_VARIABLE)
    {
      x_msgf(X_FERR, ec_nodes[n.n_lhs].n_token, "it is NOT an lvalue");
    }
    
    reg = ec_value_to_register(lvalue);
    fprintf(ec_asmout, "  %s ", n.n_type == ECN_SUFDEC ? "dec" : "inc");
    ec_value_print(lvalue);
    fprintf(ec_asmout, "\n");
    
    return reg;
  }
  else if (ECN_BEG_PREFIX < n.n_type && n.n_type < ECN_END_PREFIX)
  {
    static const char *opcodes[] = { "neg", "abs", "not" };
    
    Ec_Value rvalue;
    u32 subtype;
    
    subtype = n.n_type - ECN_BEG_MATH - 1;
    
    rvalue = ec_compile_expression(n.n_rhs);
    
    if (rvalue.v_kind == EC_VALUE_CONSTANT)
    {
      i64 y;
      
      y = rvalue.v_data.as_i64;
    
      switch (n.n_type)
      {
      case ECN_NEG: y = -y; break;
      case ECN_ABS: y = (y < 0) ? -y : y; break;
      case ECN_NOT: y = ~y; break;
      }
      
      result.v_kind = EC_VALUE_CONSTANT;
      result.v_data.as_i64 = y;
      return result;
    }
    
    result = ec_value_to_register(rvalue);
    
    fprintf(ec_asmout, "  %s ", opcodes[subtype]);
    ec_value_print(result);
    fprintf(ec_asmout, "\n");
  }
  else
  {
    x_msgf(X_FERR, n.n_token, "unimplemented feature");
  }
  
  return result;
}
/*************************************************************************************************/
Ec_Value ec_compile_function_call(Ec_Node n)
{
  Ec_Value return_value, argaddr, argval;
  u32 si, ni_list, ni_arg, the_last_one, argnum, addr;

  memset(&return_value, 0, sizeof(return_value));

  si = ec_symbols_find_you_must(ec_nodes[n.n_lhs].n_token);

  if (ec_symtab[si].st_kind != EC_SYM_FUNCTION)
  {
    x_msgf(X_ERR, ec_nodes[n.n_lhs].n_token, "it is NOT a function");
    x_msgf(X_NOTE, ec_symtab[si].st_name, "previous declaration");
    exit(EXIT_FAILURE);
  }
  
  ni_list = n.n_rhs;
  
  argnum = 0;
  addr = ec_symtab[si].st_value.as_func.f_addrbase;
  
  argaddr.v_kind = EC_VALUE_ADDRESS;
  
  for (the_last_one = 0; ni_list != 0 && !the_last_one; )
  {
    if (x_tokstr(ec_nodes[ni_list].n_token)[0] == ',')
    {
      ni_arg = ec_nodes[ni_list].n_lhs;
      ni_list = ec_nodes[ni_list].n_rhs;
    }
    else
    {
      ni_arg = ni_list;
      the_last_one = 1;
    }
    
    argval = ec_compile_expression(ni_arg);
    argaddr.v_data.as_address = ++addr;
    
    fprintf(ec_asmout, "  mov ");
    ec_value_print(argaddr);
    fprintf(ec_asmout, ", ");
    ec_value_print(argval);
    fprintf(ec_asmout, "\n");
    
    ec_value_free_register(argval);
    
    ++argnum;
  }
  
  if (argnum != ec_symtab[si].st_value.as_func.f_argnum)
  {
    x_msgf(X_ERR, ec_nodes[n.n_lhs].n_token, "wrong amount of arguments");
    x_msgf(X_NOTE, ec_symtab[si].st_name, "previous declaration");
    exit(EXIT_FAILURE);
  }
  
  fprintf(ec_asmout, "  call %.*s\n", x_toklen(ec_nodes[n.n_lhs].n_token),
                                      x_tokstr(ec_nodes[n.n_lhs].n_token));
  
  if (ec_symtab[si].st_value.as_func.f_retval)
  {
    return_value.v_kind = EC_VALUE_ADDRESS;
    return_value.v_data.as_address = ec_symtab[si].st_value.as_func.f_addrbase;
  }
  
  return return_value;
}
/*************************************************************************************************/
void ec_compile_logical_and_item(u32 ni, u32 notitem, u32 label)
{
  static const char *bropcodes[EC_BRANCH_COUNT] = { "jl ", "jle", "je ", "jne", "jg ", "jge" };

  Ec_Value lvalue, rvalue;
  Ec_Node n;
  u32 br;

  notitem = !notitem; /* because we are jumping to the else branch */
  
  n = ec_nodes[ni];
  
  switch (n.n_type)
  {
  case ECN_EQ: br = EC_BRANCH_JE;  break;
  case ECN_NE: br = EC_BRANCH_JNE; break;
  case ECN_GT: br = EC_BRANCH_JG;  break;
  case ECN_GE: br = EC_BRANCH_JGE; break;
  case ECN_LT: br = EC_BRANCH_JL;  break;
  case ECN_LE: br = EC_BRANCH_JLE; break;
  default:
    br = EC_BRANCH_JNE;
    
    lvalue = ec_compile_expression(ni);
    
    if (notitem)
    {
      br = ec_branch_reverse(br);
    }
    
    fprintf(ec_asmout, "  %s "LABEL_FORMAT", ", bropcodes[br], label);
    ec_value_print(lvalue);
    fprintf(ec_asmout, ", 0\n");
    
    ec_value_free_register(lvalue);
    
    return;
  }
  
  lvalue = ec_compile_expression(n.n_lhs);
  rvalue = ec_compile_expression(n.n_rhs);
  
  if (notitem)
  {
    br = ec_branch_reverse(br);
  }
  
  fprintf(ec_asmout, "  %s "LABEL_FORMAT", ", bropcodes[br], label);
  ec_value_print(lvalue);
  fprintf(ec_asmout, ", ");
  ec_value_print(rvalue);
  fprintf(ec_asmout, "\n");
  
  ec_value_free_register(lvalue);
  ec_value_free_register(rvalue);
}
/*************************************************************************************************/
void ec_compile_logical_and_list(u32 ni, u32 notlist, u32 notitem, u32 label, u32 is_or)
{
  u32 ci, reverse, last_item, prev_label, type;
  
  type = is_or ? ECN_LOR : ECN_LAND;
  
  if (notlist)
  {
    prev_label = label;
    label = ++ec_next_label_index;
  }
  
  last_item = 0;
  
  while (!last_item)
  {
    if (ec_nodes[ni].n_type == type)
    {
      ci = ec_nodes[ni].n_lhs;
      ni = ec_nodes[ni].n_rhs;
    }
    else
    {
      ci = ni;
      last_item = 1;
    }
    
    ci = ec_nodes_unnot(ci, &reverse);
    
    switch (ec_nodes[ci].n_type)
    {
    case ECN_LAND: ec_compile_logical_and_list(ci,  reverse ^ notitem, 0, label, 0); break;
    case ECN_LOR:  ec_compile_logical_and_list(ci, !reverse ^ notitem, 1, label, 1); break;
    default: ec_compile_logical_and_item(ci, reverse ^ notitem, label); break;
    }
  }
  
  if (notlist)
  {
    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", prev_label);
    fprintf(ec_asmout, LABEL_FORMAT":\n", label);
  }
}
/*************************************************************************************************/
u32 ec_compile_statement(u32 break_label, u32 continue_label)
{
  u32 is_return;
  
  is_return = 0;

start:

  /**/ if (ec_compile_declaration(EC_DECL_LOCAL))
  {
  }
  else if (x_accept("{"))
  {
    i32 top;
    
    ++ec_current_scope;
    top = ec_symtab_top;
  
    while (!x_accept("}"))
    {
      is_return |= ec_compile_statement(break_label, continue_label);
    }
    
    ec_symtab_top = top;
    --ec_current_scope;
  }
  else if (x_verify("}"))
  {
    return 0;
  }
  else if (x_accept(";"))
  {
  }
  else if (x_accept("if"))
  {
    u32 ni, else_label, quit_label, notlist;
    u32 ntop;
  
    ntop = ec_nodes_top;
  
    x_expect("(");
    ni = ec_expression_parse_bp(0);
    x_expect(")");
    
    else_label = ++ec_next_label_index;
    
    ni = ec_nodes_unnot(ni, &notlist);
    ec_compile_logical_and_list(ni, notlist, 0, else_label, 0);
    
    ec_nodes_top = ntop;
    
    is_return = ec_compile_statement(break_label, continue_label);
    
    if (x_accept("else"))
    {
      quit_label = ++ec_next_label_index;
      
      fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", quit_label);
      fprintf(ec_asmout, LABEL_FORMAT":\n", else_label);
    
      is_return &= ec_compile_statement(break_label, continue_label);
      
      fprintf(ec_asmout, LABEL_FORMAT":\n", quit_label);
    }
    else
    {
      fprintf(ec_asmout, LABEL_FORMAT":\n", else_label);
      is_return = 0;
    }
  }
  else if (x_accept("while"))
  {
    u32 ni, new_continue_label, new_break_label, notlist;
    u32 ntop;
  
    new_continue_label = ++ec_next_label_index;
    new_break_label = ++ec_next_label_index;
  
    ntop = ec_nodes_top;
  
    x_expect("(");
    ni = ec_expression_parse_bp(0);
    x_expect(")");
    
    fprintf(ec_asmout, LABEL_FORMAT":\n", new_continue_label);
    ni = ec_nodes_unnot(ni, &notlist);
    ec_compile_logical_and_list(ni, notlist, 0, new_break_label, 0);
    
    ec_nodes_top = ntop;
    
    ec_compile_statement(new_break_label, new_continue_label);
    
    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", new_continue_label);
    fprintf(ec_asmout, LABEL_FORMAT":\n", new_break_label);
  }
  else if (x_accept("do"))
  {
    u32 ni, new_continue_label, new_break_label, loop_label, notlist;
    u32 ntop;
    
    new_continue_label = ++ec_next_label_index;
    new_break_label = ++ec_next_label_index;
    loop_label = ++ec_next_label_index;
    
    fprintf(ec_asmout, LABEL_FORMAT":\n", loop_label);
    
    ec_compile_statement(new_break_label, new_continue_label);
    
    fprintf(ec_asmout, LABEL_FORMAT":\n", new_continue_label);
    
    ntop = ec_nodes_top;
    
    x_expect("while");
    x_expect("(");
    ni = ec_expression_parse_bp(0);
    x_expect(")");
    x_expect(";");
    
    ni = ec_nodes_unnot(ni, &notlist);
    ec_compile_logical_and_list(ni, !notlist, 0, loop_label, 0);
    
    ec_nodes_top = ntop;
    
    fprintf(ec_asmout, LABEL_FORMAT":\n", new_break_label);
  }
  else if (x_accept("for"))
  {
    u32 ni_init, ni_cond, ni_reinit;
    u32 new_continue_label, new_break_label, skip_reinit_label;
    u32 ntop;
    
    new_continue_label = ++ec_next_label_index;
    new_break_label = ++ec_next_label_index;
    skip_reinit_label = ++ec_next_label_index;

    ni_init = 0;
    ni_cond = 0;
    ni_reinit = 0;
    
    x_expect("(");
    
    if (!x_verify(";"))
    {
      ntop = ec_nodes_top;
      ni_init = ec_expression_parse_bp(0);
      ec_compile_expression(ni_init);
      ec_nodes_top = ntop;
    }
    
    x_expect(";");
    
    ntop = ec_nodes_top;
    
    if (!x_verify(";"))
    {
      ni_cond = ec_expression_parse_bp(0);
    }
    
    x_expect(";");
    
    if (!x_verify(")"))
    {
      ni_reinit = ec_expression_parse_bp(0);
      fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", skip_reinit_label);
    }
    
    fprintf(ec_asmout, LABEL_FORMAT":\n", new_continue_label);
    
    if (ni_reinit != 0)
    {
      ec_compile_expression(ni_reinit);
      fprintf(ec_asmout, LABEL_FORMAT":\n", skip_reinit_label);
    }
    
    if (ni_cond != 0)
    {
      u32 notlist;
    
      ni_cond = ec_nodes_unnot(ni_cond, &notlist);
      ec_compile_logical_and_list(ni_cond, notlist, 0, new_break_label, 0);
    }
    
    ec_nodes_top = ntop;
    
    x_expect(")");
    
    ec_compile_statement(new_break_label, new_continue_label);
    
    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", new_continue_label);
    fprintf(ec_asmout, LABEL_FORMAT":\n", new_break_label);
  }
  else if (x_accept("break"))
  {
    x_expect(";");
    
    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", break_label);
  }
  else if (x_accept("continue"))
  {
    x_expect(";");
    
    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", continue_label);
  }
  else if (x_accept("goto"))
  {
    X_Token name;
    Ec_Label *label;
  
    name = x_expect(" A");
    x_expect(";");
    
    label = ec_local_label_declare(name);
    fprintf(ec_asmout, "  jmp "LABEL_FORMAT"\n", label->l_id);
  }
  else if (x_accept("return"))
  {
    if (!x_verify(";"))
    {
      Ec_Value return_value;
      u32 ni, ntop;
      
      ntop = ec_nodes_top;
      
      ni = ec_expression_parse_bp(0);
      
      return_value = ec_compile_expression(ni);
      
      fprintf(ec_asmout, "  mov ");
      ec_value_print(ec_current_function_return_address);
      fprintf(ec_asmout, ", ");
      ec_value_print(return_value);
      fprintf(ec_asmout, "\n");
      
      ec_value_free_register(return_value);
      
      ec_nodes_top = ntop;
    }
  
    x_expect(";");
    fprintf(ec_asmout, "  ret\n");
    is_return = 1;
  }
  else
  {
    u32 ni, ntop;
    
    ntop = ec_nodes_top;
    
    ni = ec_expression_parse_bp(0);
    
    if (ec_nodes[ni].n_type == ECN_NAME && x_accept(":"))
    {
      ec_local_label_define(ec_nodes[ni].n_token);
      ec_nodes_top = ntop;
      goto start;
    }
    
    x_expect(";");
    
    ec_value_free_register(ec_compile_expression(ni));
    
    ec_nodes_top = ntop;
  }
  
  return is_return;
}
/*************************************************************************************************/
i64 ec_const_expression_evaluate(u32 ni)
{
  Ec_Node n;
  
  n = ec_nodes[ni];
  
  if (n.n_type == ECN_NEG || n.n_type == ECN_ABS || n.n_type == ECN_NOT || n.n_type == ECN_LNOT)
  {
    i64 y;
  
    y = ec_const_expression_evaluate(n.n_rhs);
    
    switch (n.n_type)
    {
    case ECN_NEG:  y = -y;               break;
    case ECN_ABS:  y = (y < 0) ? -y : y; break;
    case ECN_NOT:  y = ~y;               break;
    case ECN_LNOT: y = !y;               break;
    }
    
    return y;
  }
  else if ( (ECN_BEG_MATH < n.n_type && n.n_type < ECN_END_CONSTEXPR)
         || (ECN_BEG_LOGIC < n.n_type && n.n_type < ECN_END_LOGIC) )
  {
    i64 y, x;

    y = ec_const_expression_evaluate(n.n_lhs);
    x = ec_const_expression_evaluate(n.n_rhs);
    
    if ((n.n_type == ECN_DIV || n.n_type == ECN_MOD) && x == 0)
    {
      x_msgf(X_FERR, n.n_token, "division by zero");
    }
    
    switch (n.n_type)
    {
    case ECN_ADD:  y += x;     break;
    case ECN_SUB:  y -= x;     break;
    case ECN_MUL:  y *= x;     break;
    case ECN_DIV:  y /= x;     break;
    case ECN_MOD:  y %= x;     break;
    case ECN_SHL:  y <<= x;    break;
    case ECN_SHR:  y >>= x;    break;
    case ECN_AND:  y &= x;     break;
    case ECN_OR:   y |= x;     break;
    case ECN_XOR:  y ^= x;     break;
    case ECN_LOR:  y = y || x; break;
    case ECN_LAND: y = y && x; break;
    case ECN_LT:   y = y < x;  break;
    case ECN_LE:   y = y <= x; break;
    case ECN_GT:   y = y > x;  break;
    case ECN_GE:   y = y >= x; break;
    case ECN_EQ:   y = y == x; break;
    case ECN_NE:   y = y != x; break;
    }
  
    return y;
  }
  else if (n.n_type == ECN_TERNARY)
  {
    Ec_Node colon;
    i64 y;
    
    colon = ec_nodes[n.n_rhs];
    y = ec_const_expression_evaluate(n.n_lhs);
    return ec_const_expression_evaluate(y ? colon.n_lhs : colon.n_rhs);
  }
  else if (n.n_type == ECN_NAME)
  {
    u32 si;
    
    si = ec_symbols_find_you_must(n.n_token);
    
    if (ec_symtab[si].st_kind != EC_SYM_CONSTANT)
    {
      x_msgf(X_FERR, n.n_token, "it is NOT a constant");
    }
  
    return ec_symtab[si].st_value.as_i64;
  }
  else if (n.n_type == ECN_INTEGER)
  {
    return x_parse64(n.n_token);
  }
  else if (n.n_type == ECN_CHARACTER)
  {
    i32 size;
    char *data;
    
    size = 0;
    data = x_decode(n.n_token, &size);
    
    if (size != 0)
    {
      x_msgf(X_FERR, n.n_token, "invalid character constant");
    }
  
    return data[0];
  }
  else
  {
    x_msgf(X_FERR, n.n_token, "you can't do this in a constexpr");
  }
  
  return 0;
}
/*************************************************************************************************/
u32 ec_expression_parse_bp(u32 min_bp)
{
#define ATOM(selector, type)          {(selector), 0,           0,           (type)}
#define PREF(selector, type)          {(selector), 100,         100,         (type)}
#define SUFF(selector, type)          {(selector), 200,         200,         (type)}
#define LEFT(selector, type, bp_base) {(selector), (bp_base)-1, (bp_base)+1, (type)}
#define RGHT(selector, type, bp_base) {(selector), (bp_base)+1, (bp_base)-1, (type)}
  static const Ec_Operator atmtab[] = {
    ATOM(" A", ECN_NAME), ATOM(" D", ECN_INTEGER), ATOM(" \'", ECN_CHARACTER),
    ATOM(" Q", ECN_STRING)
  }, suftab[] = {
    PREF("(", ECN_CALL), PREF("[", ECN_AT), PREF("++", ECN_SUFINC), PREF("--", ECN_SUFDEC)
  }, pretab[] = {
    PREF("!", ECN_LNOT), PREF("~", ECN_NOT), PREF("-", ECN_NEG), PREF("+", ECN_ABS),
    PREF("++", ECN_PREINC), PREF("--", ECN_PREDEC)
  }, bintab[] = {
    RGHT(",", ECN_COMMA, 2),
    RGHT("=", ECN_ASSIGN, 5), RGHT("><", ECN_XCHG, 5),  RGHT("|<=", ECN_MINS, 5),
    RGHT("|>=", ECN_MAXS, 5), RGHT("+=", ECN_ADDS, 5),  RGHT("-=", ECN_SUBS, 5),
    RGHT("*=", ECN_MULS, 5),  RGHT("/=", ECN_DIV, 5),  RGHT("%=", ECN_MODS, 5),
    RGHT("?", ECN_TERNARY, 10),
    RGHT("||", ECN_LOR, 15), LEFT("&&", ECN_LAND, 20),
    LEFT("<", ECN_LT, 25),  LEFT("<=", ECN_LE, 25), LEFT(">", ECN_GT, 25),  LEFT(">=", ECN_GE, 25),
    LEFT("==", ECN_EQ, 25), LEFT("!=", ECN_NE, 25),
    LEFT("|<", ECN_MIN, 30), LEFT("|>", ECN_MAX, 30),
    LEFT("|", ECN_OR, 35),  LEFT("&", ECN_AND, 40),  LEFT("^", ECN_XOR, 45),
    LEFT("<<", ECN_SHL, 50), LEFT(">>", ECN_SHR, 50),
    LEFT("+", ECN_ADD, 55),  LEFT("-", ECN_SUB, 55),
    LEFT("*", ECN_MUL, 60),  LEFT("/", ECN_DIV, 60),  LEFT("%", ECN_MOD, 60)
  };

  const Ec_Operator *op;
  X_Token token, mtoken;
  u32 lhs, rhs, mhs;
  
  /**/ if ((op = ec_operator_find(atmtab, numof(atmtab))) != NULL)
  {
    lhs = ec_nodes_create_one(x_read(), op->o_type, 0, 0);
  }
  else if ((op = ec_operator_find(pretab, numof(pretab))) != NULL)
  {
    token = x_read();
    rhs = ec_expression_parse_bp(op->o_rbp);
    lhs = ec_nodes_create_one(token, op->o_type, 0, rhs);
  }
  else
  {
    x_expect("(");
    lhs = ec_expression_parse_bp(0);
    x_expect(")");
  }
  
  for (;;)
  {
    if ((op = ec_operator_find(suftab, numof(suftab))) != NULL)
    {
      if (op->o_lbp < min_bp)
      {
        break;
      }
      
      token = x_read();
      
      if (x_equals_mem(token, 1, "[") || x_equals_mem(token, 1, "("))
      {
        const char *end;
        
        end = (x_tokstr(token)[0] == '(') ? ")" : "]";
        
        if (!x_verify(end))
        {
          rhs = ec_expression_parse_bp(0);
        }
        else
        {
          rhs = 0;
        }
        
        x_expect(end);
        lhs = ec_nodes_create_one(token, op->o_type, lhs, rhs);
      }
      else
      {
        lhs = ec_nodes_create_one(token, op->o_type, lhs, 0);
      }
    }
    
    if ((op = ec_operator_find(bintab, numof(bintab))) != NULL)
    {
      if (op->o_lbp < min_bp)
      {
        break;
      }
      
      token = x_read();
      
      if (x_equals_mem(token, 1, "?"))
      {
        mhs = ec_expression_parse_bp(0);
        mtoken = x_expect(":");
        rhs = ec_expression_parse_bp(op->o_rbp);
        rhs = ec_nodes_create_one(mtoken, ECN_NULL, mhs, rhs);
        lhs = ec_nodes_create_one(token, op->o_type, lhs, rhs);
      }
      else
      {
        rhs = ec_expression_parse_bp(op->o_rbp);
        lhs = ec_nodes_create_one(token, op->o_type, lhs, rhs);
      }
      
      continue;
    }
    
    break;
  }
  
  return lhs;
}
/*************************************************************************************************/
Ec_Label *ec_local_label_declare(X_Token name)
{
  u32 li;
  
  for (li = 0; li < ec_local_labels_num; ++li)
  {
    if (x_equals(ec_local_labels[li].l_name, name))
    {
      return &ec_local_labels[li];
    }
  }
  
  if (ec_local_labels_num >= numof(ec_local_labels))
  {
    x_msgf(X_FERR, name, "too many labels");
  }
  
  ++ec_local_labels_num;
  ec_local_labels[li].l_name = name;
  ec_local_labels[li].l_id = ++ec_next_label_index;
  ec_local_labels[li].l_is_defined = 0;
  
  return &ec_local_labels[li];
}
/*************************************************************************************************/
void ec_local_label_define(X_Token name)
{
  Ec_Label *label;
  
  label = ec_local_label_declare(name);
  label->l_is_defined = 1;
  label->l_name = name;
  fprintf(ec_asmout, LABEL_FORMAT":\n", label->l_id);
}
/*************************************************************************************************/
u32 ec_nodes_create_one(X_Token token, u8 type, u8 lhs, u8 rhs)
{
  ++ec_nodes_top;
  
  if (ec_nodes_top >= numof(ec_nodes))
  {
    x_msgf(X_FERR, token, "out of memory");
  }
  
  ec_nodes[ec_nodes_top].n_token = token;
  ec_nodes[ec_nodes_top].n_type = type;
  ec_nodes[ec_nodes_top].n_lhs = lhs;
  ec_nodes[ec_nodes_top].n_rhs = rhs;
  
  return ec_nodes_top;
}
/*************************************************************************************************/
u32 ec_nodes_unnot(u32 ni, u32 *is_not_out)
{
  u32 num;

  for (num = 0; ec_nodes[ni].n_type == ECN_LNOT; ++num)
  {
    ni = ec_nodes[ni].n_rhs;
  }
  
  *is_not_out = (num % 2 != 0);
  
  return ni;
}
/*************************************************************************************************/
const Ec_Operator *ec_operator_find(const Ec_Operator *tab, u32 num)
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
i32 ec_symbols_find(X_Token name)
{
  i32 i;
  
  for (i = ec_symtab_top; i > 0; --i)
  {
    if (x_equals(ec_symtab[i].st_name, name))
    {
      return i;
    }
  }
  
  return -1;
}
/*************************************************************************************************/
u32 ec_symbols_find_you_must(X_Token name)
{
  i32 i;
  
  if ((i = ec_symbols_find(name)) < 0)
  {
    x_msgf(X_FERR, name, "unknown name");
  }
  
  return i;
}
/*************************************************************************************************/
u32 ec_symbols_declare(X_Token name, u16 kind, u16 flags)
{
  i32 si;
  
  si = ec_symbols_find(name);
  
  if (si >= 0)
  {
    if (kind != EC_SYM_FUNCTION && ec_symtab[si].st_scope < ec_current_scope)
    {
      goto create_symbol;
    }
  
    if ((ec_symtab[si].st_kind != kind)
     || (ec_symtab[si].st_flags & flags & EC_FLG_DEFINED))
    {
      x_msgf(X_ERR, name, "symbol redefinition");
      x_msgf(X_NOTE, ec_symtab[si].st_name, "previous declaration");
      exit(EXIT_FAILURE);
    }
    
    ec_symtab[si].st_flags |= flags;
    
    if (flags & EC_FLG_DEFINED)
    {
      ec_symtab[si].st_name = name;
    }
    
    return si;
  }
  
create_symbol:
  si = ++ec_symtab_top;
  
  memset(&ec_symtab[si], 0, sizeof(ec_symtab[si]));
  ec_symtab[si].st_kind = kind;
  ec_symtab[si].st_name = name;
  ec_symtab[si].st_flags = flags;
  
  return si;
}
/*************************************************************************************************/
void ec_value_free_register(Ec_Value value)
{
  if (value.v_kind != EC_VALUE_REGISTER)
  {
    return;
  }
  
  ec_regs_taken &= ~(1 << value.v_data.as_regi);
}
/*************************************************************************************************/
Ec_Value ec_value_from_symbol(u32 si)
{
  Ec_Value result;
  
  memset(&result, 0, sizeof(result));

  /**/ if (ec_symtab[si].st_kind == EC_SYM_CONSTANT)
  {
    result.v_kind = EC_VALUE_CONSTANT;
    result.v_data.as_i64 = ec_symtab[si].st_value.as_i64;
  }
  else if (ec_symtab[si].st_kind == EC_SYM_VARIABLE)
  {
    result.v_kind = EC_VALUE_VARIABLE;
    result.v_data.as_symi = si;
  }
  
  return result;
}
/*************************************************************************************************/
void ec_value_print(Ec_Value value)
{
  const Ec_Symbol *sym;

  switch (value.v_kind)
  {
  case EC_VALUE_CONSTANT:
  fprintf(ec_asmout, "%i", (i32)value.v_data.as_i64);
  break;
  case EC_VALUE_REGISTER:
  fprintf(ec_asmout, "R%02i", value.v_data.as_regi);
  break;
  case EC_VALUE_ADDRESS:
  fprintf(ec_asmout, "V%04X", value.v_data.as_address);
  break;
  case EC_VALUE_VARIABLE:
  sym = &ec_symtab[value.v_data.as_symi];
  if (sym->st_flags & EC_FLG_EXTERN)
  {
    fprintf(ec_asmout, "%.*s", x_toklen(sym->st_name), x_tokstr(sym->st_name));
  }
  else
  {
    fprintf(ec_asmout, "V%04X", sym->st_value.as_address);
  }
  break;
  case EC_VALUE_FUNCTION:
  /* TODO: print the corresponding label */
  break;
  }
}
/*************************************************************************************************/
Ec_Value ec_value_allocate_register(void)
{
  Ec_Value inreg;
  u64 mask;
  u32 regi;
  
  for (regi = 0; regi < 64 && (1 << regi) & ec_regs_taken; ++regi) /**/;
  
  if (regi >= 64)
  {
    x_msgf(X_FERR, 0, "out of registers");
  }
  
  mask = 1 << regi;
  
  if ((ec_regs_declared & mask) == 0)
  {
    ec_regs_declared |= mask;
    fprintf(ec_asmout, "eter R%02i\n", regi);
  }
  
  ec_regs_taken |= mask;
  
  memset(&inreg, 0, sizeof(inreg));
  inreg.v_kind = EC_VALUE_REGISTER;
  inreg.v_data.as_regi = regi;
  
  return inreg;
}
/*************************************************************************************************/
Ec_Value ec_value_to_register(Ec_Value value)
{
  Ec_Value inreg;
  
  if (value.v_kind == EC_VALUE_REGISTER)
  {
    return value;
  }
  
  inreg = ec_value_allocate_register();
  
  return ec_value_to_the_register(value, inreg);
}
/*************************************************************************************************/
Ec_Value ec_value_to_the_register(Ec_Value value, Ec_Value inreg)
{
  fprintf(ec_asmout, "  mov ");
  ec_value_print(inreg);
  fprintf(ec_asmout, ", ");
  ec_value_print(value);
  fprintf(ec_asmout, "\n");
  
  return inreg;
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

