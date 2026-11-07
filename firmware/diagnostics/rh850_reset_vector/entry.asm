; RAM-only debugger entry. This file has no code-flash reset section.
    .section "RV_ENTRY", text
    .align 4
    .public _RvEntry
    .extern _RvMain
_RvEntry:
    di
    mov #_RvStackTop, sp
    mov 0, gp
    mov 0, ep
    ; Initialize only our own BSS/stack/mailbox, not debugger or FCL copy RAM.
    mov #__s.bss, r6
    mov #__e.bss, r7
    jarl _RvClear, lp
    mov #__sR_FCL_DATA.bss, r6
    mov #__eR_FCL_DATA.bss, r7
    jarl _RvClear, lp
    mov #__sRV_STACK.bss, r6
    mov #__eRV_STACK.bss, r7
    jarl _RvClear, lp
    mov #__sRV_MAILBOX.bss, r6
    mov #__eRV_MAILBOX.bss, r7
    jarl _RvClear, lp
    mov 0xFEDE0200, r10
    ldsr r10, 3, 1
    stsr 5, r10, 0
    mov 0x00008020, r11
    or r11, r10
    ldsr r10, 5, 0
    syncp
    jarl32 _RvMain, lp
_RvHalt:
    br _RvHalt

_RvClear:
    cmp r6, r7
    bnh _RvClearDone
    st.w r0, 0[r6]
    add 4, r6
    br _RvClear
_RvClearDone:
    jmp [lp]

    .public _RvReadRbase
_RvReadRbase:
    stsr 2, r10, 1
    jmp [lp]

_RvTrap:
    di
    mov 0xFEDF7000, r10
    mov 0xF0, r11
    st.w r11, 4[r10]
    stsr 2, r11, 0
    st.w r11, 0xB0[r10]
    stsr 14, r11, 0
    st.w r11, 0xB4[r10]
    syncp
    br _RvHalt

    .section "RV_VECTORS", text
    .align 512
    .rept 32
    .align 16
    jr32 _RvTrap
    .endm

    .section "RV_STACK.bss", bss
    .align 4
    .ds 0x1000
_RvStackTop:

    .section "RV_FCL_COPY.bss", bss
    .align 4
    .ds 0x7000

    .section ".bss", bss
    .align 4
    .ds 4
