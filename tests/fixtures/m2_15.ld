SECTIONS
{
  . = 0;
  .text : { *(.text*) }
  .data . : SUBALIGN(1) { *(.data*) }
  /DISCARD/ : { *(.comment) *(.note*) }
}
