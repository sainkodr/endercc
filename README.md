# EnderC Compiler

Compile programs to Minecraft commands!

## Build Instructions

All you need is a C compiler to build the project.

```sh
cc -std=c89 -pedantic -pedantic-errors -o enderasm enderasm.c
cc -std=c89 -pedantic -pedantic-errors -o ednercc endercc.c
```

## Usage Instructions

Compile and assemble the program.

```sh
ednercc -o main.s main.endc
enderasm -o "my_datapack/data/namespace/functions" -p "namespace:" main.s
```

Run the functions via `/function` and `/schedule` commands or by referncing them in tags,
advancements, or enchantments.

## EnderASM

EnderASM is an assembly language, designed to be assembled to Minecraft commands and run as an
MCFunction in Minecraft datapack.

The file extension for EnderASM source files is `.s`.

See [EnderASM Examples](./enderasm_examples/).

## EnderC

EnderC is a C-like programming language, designed to be compiled to Minecraft commands and run as
an MCFunction in Minecraft datapack.

The file extension for EnderC source files is `.endc`.

## EnderASM Assembler

Takes a program written in EnderASM and generates the corresponding Minecraft commands.
The resulting commands are spread across several `.mcfunction` files.

See [enderasm_manual.md](./enderasm_manual.md) for details.

## EnderC Compiler

Takes a program written in EnderC and generates the corresponding EnderASM equivalent that can be
assembled to Minecraft commands.

## Pull Requests

I'm not sure...

## License

EnderC Compiler's source code is released under The Unlicense.
See [LICENSE](./LICENSE) for details.

