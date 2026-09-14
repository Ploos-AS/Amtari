SECTIONS
{
  . = 0;
  .text : { *(.text*) }
  . = ALIGN(2);
  .data : SUBALIGN(2) { *(.data*) }
  /DISCARD/ : { *(.comment) *(.note*) }
}
