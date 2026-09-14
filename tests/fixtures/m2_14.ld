SECTIONS
{
  . = 0;
  .text : { *(.text.startup) *(.text*) }
  . = ALIGN(2);
  .data : SUBALIGN(2) { *(.data*) }
  . = ALIGN(2);
  .bss : SUBALIGN(2) { *(.bss*) *(COMMON) }
}
