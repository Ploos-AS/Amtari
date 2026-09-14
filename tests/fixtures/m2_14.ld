SECTIONS
{
  . = 0;
  .text : { *(.text.startup) *(.text*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
