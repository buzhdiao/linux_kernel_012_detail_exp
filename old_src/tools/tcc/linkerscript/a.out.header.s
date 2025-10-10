	.section .a.out.header, "a", @progbits
	.align 32
	.size	header, 32
header:
	.long	267       /* ZMAGIC */
	.long	.textsize /* a_text */
	.long	.datasize /* a_data */
	.long	.bsssize  /* a_bss */
	.long	.symsize  /* a_syms */
	.long	0         /* a_entry */
	.long	0         /* a_trsize */
	.long	0         /* a_drsize */
