# 16-bit disassembly inventory

- Code segments disassembled: 78
- Code bytes disassembled: 309,486
- Function-entry candidates: 1,094
- Named/exported entry points: 21
- Relocation records available for annotation: 7,285

Function candidates combine the NE entry table, initial CS:IP, relocated far CALL/JMP targets, and direct near CALL targets. Because the binary has no full symbol table, candidates are evidence for audit and naming rather than a claim that every byte range is a distinct source-level function.

Per-segment `.asm` and `.bin` files are kept under `original/disassembly/segments` with the supplied copyrighted binary.
