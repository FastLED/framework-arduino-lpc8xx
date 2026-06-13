ENTRY(Reset_Handler)

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))  /* Vector table from startup C file */
    . = ALIGN(4);
  } > FLASH

  .crp 0x000002FC :
  {
    KEEP(*(.crp))
  } > FLASH = 0xFF

  .text :
  {
    KEEP(*(.after_vectors))
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    KEEP(*(.preinit_array*))
    KEEP(*(.init_array*))
    KEEP(*(.fini_array*))
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  /*
   * ARM exception index. Even on Cortex-M0+ targets that don't unwind,
   * `-flto` + the `__udivmoddi4`/`__udivdi3` helpers from libgcc emit
   * `.ARM.exidx.*` records that ld will refuse to merge unless placed
   * explicitly. Without this section the LMA defaults overlap `.data`
   * and the link errors with `LMA [...] overlaps section .data LMA [...]`.
   */
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } > FLASH

  _sidata = LOADADDR(.data);

  .data : AT(_etext + SIZEOF(.ARM.exidx))
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /*
   * Heap. newlib-nano's nosys `_sbrk` grows the heap upward from the `end`
   * symbol toward the descending stack, so dynamic allocation (malloc, and
   * `operator new`/`new[]` we provide in `lpc8xx_runtime.cpp`) needs
   * `end`/`_end` defined at the top of `.bss`.
   *
   * These MUST be forced assignments, not `PROVIDE`: any object-file weak
   * definition of `end` (e.g. one that previously lived in `lpc8xx_runtime.cpp`
   * as a link-fallback, or one synthesized by LTO partitioning) is sufficient
   * to suppress `PROVIDE(end)`. The resulting `end` then resolves to whatever
   * address that weak symbol got placed at — typically *inside* `.bss` — and
   * `_sbrk` starts the heap on top of live `.bss` data. The first
   * heap-growing `malloc` then corrupts `.bss` (including `_sbrk`'s own
   * `heap_end` state and any polymorphic singleton vptrs) and the core
   * HardFaults. A forced assignment overrides any object symbol and pins the
   * heap base past the real end of `.bss`.
   *
   * `__heap_start` / `__HeapBase` are FreeRTOS-convention aliases.
   */
  .heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    end = .;
    _end = .;
    __heap_start = .;
    __HeapBase = .;
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM
}
