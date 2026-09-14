SECTIONS
{
  . = 0;
  .text : { *(.text*) }
  .data : { *(.data*) }
  /DISCARD/ : { *(.comment) *(.note*) }
}
