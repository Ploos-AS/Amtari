SECTIONS
{
  . = 0;
  .text : { *(.text.startup) *(.text*) }
  .data . : SUBALIGN(1) { *(.data*) }
  .bss . : SUBALIGN(1) { *(.bss*) *(COMMON) }
}
