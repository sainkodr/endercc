# Command-line options

## Place the output into a file

`-o <filepath>` or `--output <filepath>`

By default `a.s` is used as a file for outputing EnderASM code.

## Show version

`-v` or `--version`

## Show help

`-h` or `--help`

# EnderC Language

## Operator Precedence and Associativity

```
Precedence
 | Associativity (R = Right-To-Left, L = Left-To-Right)
 |  | Operators
 |  |  |
01  L  x(y) x[y] x++ x--
02  R  !x -x +x ~x ++x --x
03  L  * / %
04  L  + -
05  L  << >>
06  L  ^
07  L  &
08  L  |
09  L  |< |>
10  L  == != <= >= < >
11  R  &&
12  R  ||
13  R  ?:
14  R  = >< |<= |>= += -= *= /= %=
15  R  ,
```

## Operator Availability

The next operators are only available in CONST expressions:
```
| & ^ << >>
```

The next operators are only available in RUNTIME expressions:
```
= >< |<= |>= += -= *= /= %= x(y) x[y] x++ x-- ++x --x
```

The next operators are available in ALL expressions:
```
, ?: && || == != <= >= < > |< |> + - * / % !x -x +x ~x
```

## Imposter Operators

### Swap Operator

`><` (requires an lvalue from both sides)

### Minimum Operator

`|<` and `|<=`

### Maximum Operator

`|>` and `|>=`

### Absolute Value Operator

`+x`

## Intrinsics

### tellraw

```c
void tellraw(string selector, ...);
```

Assembles into the corresponding tellraw command. You can pass any number of text, variables, and
constants after a selector.

## Keywords

The keywords:

```c
do const return while continue if enum for break else goto void
```

work as in C.

## Imposter Keywords

### extern For Variables

`extern` for variables means use the name of the variable instead of its "address". EnderC compiler
creates a unique identifier for variables by default, but if for some reason you want the variable
to keep its original name you can use `extern`. Consider the next program:

```c
void foo()
{
  eter x;
  
  x = 0;
}

void bar()
{
  eter x;
  
  x = 1;
}
```

Both foo and bar refer to x but it's actually two different variables with different addresses.
But if you use `extern`:

```c
void foo()
{
  extern eter x;
  
  x = 0;
}

void bar()
{
  extern eter x;
  
  x = 1;
}
```

Now it's a reference by name and both foo and bar refer to the same variable named x. And also
when you are in the game you can access the variable by using its name in commands like
`scoreboard`, it's predictable. 

### extern For Functions

`extern` for functions means that the function is defined somewhere else or used somewhere else.
That's needed to avoid some warnings and errors for functions like `load` and `tick`. 

### eter

`eter` stands for "eternal variable", a variable that doesn't loose its value between loads, which
reflects the semantics of a scoreboard value in Minecraft. You can think about it as a `static`
variable in C but with even longer lifetime. `eter` variables are used everywhere, including for
passing function arguments and for returning a value. That affects how recurtion works. Consider
the next function:

```c
void foo(eter x)
{
  if (x > 0)
  {
    foo(x - 1);
  }
  
  tellraw("@a", x);
}
```

This function will print a sequence of zeros no matter what positive value you pass in. This is
because there is only one variable allocated for passing the argument, and each call to foo
modifies it. At the time you get to tellraw, x will already be set to zero. Think about it as
passing arguments through a global variable.

### cmd

```c
cmd(...);
```

Assemble a minecraft command. For example:

```c
const eter MY_Y = 10;
cmd("tp @a 0 ", MY_Y, " 0");
```

Assembles to:

```
tp @a 0 10 0
```

## Escapeless Text

Any text in such \`...\` quotes is treated as-is without evaluating escape sequences.
For example:

```c
cmd(`say @a "My text"`);
cmd(`say @a "\"My quoted text\""`);
```

Assembles to:

```c
say @a "My text"
say @a "\"My quoted text\""
```

