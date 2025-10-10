OUTPUT_FORMAT(binary)
SECTIONS
{
  . = 0x0;
  .text : AT(0x400) {
      PROVIDE(.text_begin = .);
      *(.text);
      PROVIDE(.text_real_end = .);
  }
  .data ALIGN(0x400) : {
      PROVIDE(.text_align_end = .);
      PROVIDE(.data_begin = .);
      *(.data)
      *(.rodata)
      *(.rodata1)
      PROVIDE(.data_real_end = .);
  }
  .bss ALIGN(0x400) : {
      PROVIDE(.data_align_end = .);
      PROVIDE(.bss_begin = .);
      *(.bss)
      PROVIDE(.bss_real_end = .);
  }
  .symtab ALIGN(0x400) : {
      PROVIDE(.sym_begin = .);
      *(.symtab)
      PROVIDE(.sym_end = .);
  }
  .text_end = (SIZEOF(.data) + SIZEOF(.bss) == 0) ? .text_real_end : .text_align_end;
  .data_end = (SIZEOF(.bss) == 0) ? .data_real_end : .data_align_end;
  .bss_end  = .bss_real_end;
  PROVIDE(.textsize = .text_end - .text_begin);
  PROVIDE(.datasize = .data_end - .data_begin);
  PROVIDE(.bsssize = .bss_end - .bss_begin);
  PROVIDE(.symsize = 0);
  .header ALIGN(0x400) : AT(0x0){
      LONG(267)
      LONG(.textsize)
      LONG(.datasize)
      LONG(.bsssize)
      LONG(0)
      LONG(_start)
      LONG(0)
  }
  /DISCARD/ : { *(.eh_frame) *(.comment) *(.rel.eh_frame) *(.note.GNU-stack) }
}
