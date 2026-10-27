ENTRY(graft_hook)
SECTIONS {
  . = 0x508000;
  .graft : { *(.text*) *(.rodata*) }
  /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame*) *(.comment*) *(.note*) }
}
