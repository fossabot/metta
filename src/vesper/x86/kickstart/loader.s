;
; Copyright 2007 - 2009, Stanislav Karchebnyy <berkus+metta@madfire.net>
;
; Distributed under the Boost Software License, Version 1.0.
; (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
;
; x86 GRUB loader.
; jump to setup_kernel in unpacker.cpp to do all the dirty job.
; this file only contains several boot-helpers in assembly.
;
global loader                          ; making entry point visible to linker
global activate_gdt
global initial_esp
extern kickstart
extern KERNEL_BASE
extern data_end
extern bss_end

; setting up the Multiboot header - see GRUB docs for details
MODULEALIGN equ  1<<0                  ; align loaded modules on page boundaries
MEMINFO     equ  1<<1                  ; provide memory map
KLUDGE      equ  1<<16
FLAGS       equ  MODULEALIGN | MEMINFO | KLUDGE
MAGIC       equ  0x1BADB002            ; 'magic number' lets bootloader find the header
CHECKSUM    equ -(MAGIC + FLAGS)       ; checksum required

bits 32                                ; 32 bit PM

section .bss
resb 0x1000
initial_stack:                         ; reserve one page for startup stack

section .multiboot_info                ; mboot header should fit in first 8KiB so we make a section for it
align 4
multiboot_header:
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd multiboot_header
    dd KERNEL_BASE
    dd data_end                        ; set load_end_addr == 0 to load whole bootloader
    dd bss_end
    dd loader

initial_esp: dd 0                      ; referencing initial_stack from C doesn't work :(

section .text
loader:
    cli
    mov esp, initial_stack
    mov [initial_esp], esp
    push ebx                           ; pass Multiboot info structure

    mov ebp, 0                         ; make base pointer NULL here so we know
                                       ; where to stop a backtrace.
    call  kickstart                    ; call startup loader code

    cli
    jmp short $                        ; halt machine should startup code return

activate_gdt:
    mov eax, [esp+4]  ; Get the pointer to the GDT, passed as a parameter.
    lgdt [eax]        ; Load the new GDT pointer
    jmp 0x08:.flush   ; 0x08 is the offset to our code segment: Far jump!
.flush:
    mov ax, 0x10      ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax        ; Load all data segment selectors
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; kate: indent-width 4; replace-tabs on;
; vim: set et sw=4 ts=4 sts=4 cino=(4 :
