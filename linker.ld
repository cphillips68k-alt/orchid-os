cat > linker.ld << 'EOF'
OUTPUT_FORMAT("binary")
ENTRY(start)
SECTIONS {
    . = 0x10000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
EOF