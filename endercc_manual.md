# Command-line options

TODO: well... write the manual

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

### Minimum Operator

`|<`

### Maximum Operator

`|>`

### Absolute Value Operator

`+x`




