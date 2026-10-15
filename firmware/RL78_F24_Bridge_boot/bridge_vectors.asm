; Generated hardware IRQ dispatch; PSW is restored by the selected ISR RETI.
.PUBLIC _F24Bridge_Jump
.EXTERN _R_Config_UART0_Stop
.EXTERN _R_Config_TAU0_1_Stop
.EXTERN _r_Config_TAU0_1_interrupt
.EXTERN _r_Config_UART0_interrupt_receive
.EXTERN _r_Config_UART0_interrupt_send
bridge_irq_04 .VECTOR 0x04
.SECTION .text, TEXT
bridge_irq_04:
    CMP !0xF710,#0xA5
    BZ $app_irq_04
    BR !!bridge_default_isr
app_irq_04:
    BR !!0x05008
bridge_irq_06 .VECTOR 0x06
.SECTION .text, TEXT
bridge_irq_06:
    CMP !0xF710,#0xA5
    BZ $app_irq_06
    BR !!bridge_default_isr
app_irq_06:
    BR !!0x0500C
bridge_irq_08 .VECTOR 0x08
.SECTION .text, TEXT
bridge_irq_08:
    CMP !0xF710,#0xA5
    BZ $app_irq_08
    BR !!bridge_default_isr
app_irq_08:
    BR !!0x05010
bridge_irq_0A .VECTOR 0x0A
.SECTION .text, TEXT
bridge_irq_0A:
    CMP !0xF710,#0xA5
    BZ $app_irq_0A
    BR !!bridge_default_isr
app_irq_0A:
    BR !!0x05014
bridge_irq_0C .VECTOR 0x0C
.SECTION .text, TEXT
bridge_irq_0C:
    CMP !0xF710,#0xA5
    BZ $app_irq_0C
    BR !!bridge_default_isr
app_irq_0C:
    BR !!0x05018
bridge_irq_0E .VECTOR 0x0E
.SECTION .text, TEXT
bridge_irq_0E:
    CMP !0xF710,#0xA5
    BZ $app_irq_0E
    BR !!bridge_default_isr
app_irq_0E:
    BR !!0x0501C
bridge_irq_10 .VECTOR 0x10
.SECTION .text, TEXT
bridge_irq_10:
    CMP !0xF710,#0xA5
    BZ $app_irq_10
    BR !!bridge_default_isr
app_irq_10:
    BR !!0x05020
bridge_irq_12 .VECTOR 0x12
.SECTION .text, TEXT
bridge_irq_12:
    CMP !0xF710,#0xA5
    BZ $app_irq_12
    BR !!bridge_default_isr
app_irq_12:
    BR !!0x05024
bridge_irq_14 .VECTOR 0x14
.SECTION .text, TEXT
bridge_irq_14:
    CMP !0xF710,#0xA5
    BZ $app_irq_14
    BR !!bridge_default_isr
app_irq_14:
    BR !!0x05028
bridge_irq_16 .VECTOR 0x16
.SECTION .text, TEXT
bridge_irq_16:
    CMP !0xF710,#0xA5
    BZ $app_irq_16
    BR !!_r_Config_UART0_interrupt_send
app_irq_16:
    BR !!0x0502C
bridge_irq_18 .VECTOR 0x18
.SECTION .text, TEXT
bridge_irq_18:
    CMP !0xF710,#0xA5
    BZ $app_irq_18
    BR !!_r_Config_UART0_interrupt_receive
app_irq_18:
    BR !!0x05030
bridge_irq_1A .VECTOR 0x1A
.SECTION .text, TEXT
bridge_irq_1A:
    CMP !0xF710,#0xA5
    BZ $app_irq_1A
    BR !!bridge_default_isr
app_irq_1A:
    BR !!0x05034
bridge_irq_1C .VECTOR 0x1C
.SECTION .text, TEXT
bridge_irq_1C:
    CMP !0xF710,#0xA5
    BZ $app_irq_1C
    BR !!bridge_default_isr
app_irq_1C:
    BR !!0x05038
bridge_irq_1E .VECTOR 0x1E
.SECTION .text, TEXT
bridge_irq_1E:
    CMP !0xF710,#0xA5
    BZ $app_irq_1E
    BR !!bridge_default_isr
app_irq_1E:
    BR !!0x0503C
bridge_irq_20 .VECTOR 0x20
.SECTION .text, TEXT
bridge_irq_20:
    CMP !0xF710,#0xA5
    BZ $app_irq_20
    BR !!bridge_default_isr
app_irq_20:
    BR !!0x05040
bridge_irq_22 .VECTOR 0x22
.SECTION .text, TEXT
bridge_irq_22:
    CMP !0xF710,#0xA5
    BZ $app_irq_22
    BR !!bridge_default_isr
app_irq_22:
    BR !!0x05044
bridge_irq_24 .VECTOR 0x24
.SECTION .text, TEXT
bridge_irq_24:
    CMP !0xF710,#0xA5
    BZ $app_irq_24
    BR !!bridge_default_isr
app_irq_24:
    BR !!0x05048
bridge_irq_26 .VECTOR 0x26
.SECTION .text, TEXT
bridge_irq_26:
    CMP !0xF710,#0xA5
    BZ $app_irq_26
    BR !!bridge_default_isr
app_irq_26:
    BR !!0x0504C
bridge_irq_28 .VECTOR 0x28
.SECTION .text, TEXT
bridge_irq_28:
    CMP !0xF710,#0xA5
    BZ $app_irq_28
    BR !!bridge_default_isr
app_irq_28:
    BR !!0x05050
bridge_irq_2A .VECTOR 0x2A
.SECTION .text, TEXT
bridge_irq_2A:
    CMP !0xF710,#0xA5
    BZ $app_irq_2A
    BR !!bridge_default_isr
app_irq_2A:
    BR !!0x05054
bridge_irq_2C .VECTOR 0x2C
.SECTION .text, TEXT
bridge_irq_2C:
    CMP !0xF710,#0xA5
    BZ $app_irq_2C
    BR !!bridge_default_isr
app_irq_2C:
    BR !!0x05058
bridge_irq_2E .VECTOR 0x2E
.SECTION .text, TEXT
bridge_irq_2E:
    CMP !0xF710,#0xA5
    BZ $app_irq_2E
    BR !!_r_Config_TAU0_1_interrupt
app_irq_2E:
    BR !!0x0505C
bridge_irq_30 .VECTOR 0x30
.SECTION .text, TEXT
bridge_irq_30:
    CMP !0xF710,#0xA5
    BZ $app_irq_30
    BR !!bridge_default_isr
app_irq_30:
    BR !!0x05060
bridge_irq_32 .VECTOR 0x32
.SECTION .text, TEXT
bridge_irq_32:
    CMP !0xF710,#0xA5
    BZ $app_irq_32
    BR !!bridge_default_isr
app_irq_32:
    BR !!0x05064
bridge_irq_34 .VECTOR 0x34
.SECTION .text, TEXT
bridge_irq_34:
    CMP !0xF710,#0xA5
    BZ $app_irq_34
    BR !!bridge_default_isr
app_irq_34:
    BR !!0x05068
bridge_irq_36 .VECTOR 0x36
.SECTION .text, TEXT
bridge_irq_36:
    CMP !0xF710,#0xA5
    BZ $app_irq_36
    BR !!bridge_default_isr
app_irq_36:
    BR !!0x0506C
bridge_irq_38 .VECTOR 0x38
.SECTION .text, TEXT
bridge_irq_38:
    CMP !0xF710,#0xA5
    BZ $app_irq_38
    BR !!bridge_default_isr
app_irq_38:
    BR !!0x05070
bridge_irq_3A .VECTOR 0x3A
.SECTION .text, TEXT
bridge_irq_3A:
    CMP !0xF710,#0xA5
    BZ $app_irq_3A
    BR !!bridge_default_isr
app_irq_3A:
    BR !!0x05074
bridge_irq_3C .VECTOR 0x3C
.SECTION .text, TEXT
bridge_irq_3C:
    CMP !0xF710,#0xA5
    BZ $app_irq_3C
    BR !!bridge_default_isr
app_irq_3C:
    BR !!0x05078
bridge_irq_3E .VECTOR 0x3E
.SECTION .text, TEXT
bridge_irq_3E:
    CMP !0xF710,#0xA5
    BZ $app_irq_3E
    BR !!bridge_default_isr
app_irq_3E:
    BR !!0x0507C
bridge_irq_40 .VECTOR 0x40
.SECTION .text, TEXT
bridge_irq_40:
    CMP !0xF710,#0xA5
    BZ $app_irq_40
    BR !!bridge_default_isr
app_irq_40:
    BR !!0x05080
bridge_irq_42 .VECTOR 0x42
.SECTION .text, TEXT
bridge_irq_42:
    CMP !0xF710,#0xA5
    BZ $app_irq_42
    BR !!bridge_default_isr
app_irq_42:
    BR !!0x05084
bridge_irq_44 .VECTOR 0x44
.SECTION .text, TEXT
bridge_irq_44:
    CMP !0xF710,#0xA5
    BZ $app_irq_44
    BR !!bridge_default_isr
app_irq_44:
    BR !!0x05088
bridge_irq_46 .VECTOR 0x46
.SECTION .text, TEXT
bridge_irq_46:
    CMP !0xF710,#0xA5
    BZ $app_irq_46
    BR !!bridge_default_isr
app_irq_46:
    BR !!0x0508C
bridge_irq_48 .VECTOR 0x48
.SECTION .text, TEXT
bridge_irq_48:
    CMP !0xF710,#0xA5
    BZ $app_irq_48
    BR !!bridge_default_isr
app_irq_48:
    BR !!0x05090
bridge_irq_4A .VECTOR 0x4A
.SECTION .text, TEXT
bridge_irq_4A:
    CMP !0xF710,#0xA5
    BZ $app_irq_4A
    BR !!bridge_default_isr
app_irq_4A:
    BR !!0x05094
bridge_irq_4C .VECTOR 0x4C
.SECTION .text, TEXT
bridge_irq_4C:
    CMP !0xF710,#0xA5
    BZ $app_irq_4C
    BR !!bridge_default_isr
app_irq_4C:
    BR !!0x05098
bridge_irq_4E .VECTOR 0x4E
.SECTION .text, TEXT
bridge_irq_4E:
    CMP !0xF710,#0xA5
    BZ $app_irq_4E
    BR !!bridge_default_isr
app_irq_4E:
    BR !!0x0509C
bridge_irq_50 .VECTOR 0x50
.SECTION .text, TEXT
bridge_irq_50:
    CMP !0xF710,#0xA5
    BZ $app_irq_50
    BR !!bridge_default_isr
app_irq_50:
    BR !!0x050A0
bridge_irq_52 .VECTOR 0x52
.SECTION .text, TEXT
bridge_irq_52:
    CMP !0xF710,#0xA5
    BZ $app_irq_52
    BR !!bridge_default_isr
app_irq_52:
    BR !!0x050A4
bridge_irq_54 .VECTOR 0x54
.SECTION .text, TEXT
bridge_irq_54:
    CMP !0xF710,#0xA5
    BZ $app_irq_54
    BR !!bridge_default_isr
app_irq_54:
    BR !!0x050A8
bridge_irq_56 .VECTOR 0x56
.SECTION .text, TEXT
bridge_irq_56:
    CMP !0xF710,#0xA5
    BZ $app_irq_56
    BR !!bridge_default_isr
app_irq_56:
    BR !!0x050AC
bridge_irq_58 .VECTOR 0x58
.SECTION .text, TEXT
bridge_irq_58:
    CMP !0xF710,#0xA5
    BZ $app_irq_58
    BR !!bridge_default_isr
app_irq_58:
    BR !!0x050B0
bridge_irq_5A .VECTOR 0x5A
.SECTION .text, TEXT
bridge_irq_5A:
    CMP !0xF710,#0xA5
    BZ $app_irq_5A
    BR !!bridge_default_isr
app_irq_5A:
    BR !!0x050B4
bridge_irq_5C .VECTOR 0x5C
.SECTION .text, TEXT
bridge_irq_5C:
    CMP !0xF710,#0xA5
    BZ $app_irq_5C
    BR !!bridge_default_isr
app_irq_5C:
    BR !!0x050B8
bridge_irq_5E .VECTOR 0x5E
.SECTION .text, TEXT
bridge_irq_5E:
    CMP !0xF710,#0xA5
    BZ $app_irq_5E
    BR !!bridge_default_isr
app_irq_5E:
    BR !!0x050BC
bridge_irq_60 .VECTOR 0x60
.SECTION .text, TEXT
bridge_irq_60:
    CMP !0xF710,#0xA5
    BZ $app_irq_60
    BR !!bridge_default_isr
app_irq_60:
    BR !!0x050C0
bridge_irq_62 .VECTOR 0x62
.SECTION .text, TEXT
bridge_irq_62:
    CMP !0xF710,#0xA5
    BZ $app_irq_62
    BR !!bridge_default_isr
app_irq_62:
    BR !!0x050C4
bridge_irq_64 .VECTOR 0x64
.SECTION .text, TEXT
bridge_irq_64:
    CMP !0xF710,#0xA5
    BZ $app_irq_64
    BR !!bridge_default_isr
app_irq_64:
    BR !!0x050C8
bridge_irq_66 .VECTOR 0x66
.SECTION .text, TEXT
bridge_irq_66:
    CMP !0xF710,#0xA5
    BZ $app_irq_66
    BR !!bridge_default_isr
app_irq_66:
    BR !!0x050CC
bridge_irq_68 .VECTOR 0x68
.SECTION .text, TEXT
bridge_irq_68:
    CMP !0xF710,#0xA5
    BZ $app_irq_68
    BR !!bridge_default_isr
app_irq_68:
    BR !!0x050D0
bridge_irq_6A .VECTOR 0x6A
.SECTION .text, TEXT
bridge_irq_6A:
    CMP !0xF710,#0xA5
    BZ $app_irq_6A
    BR !!bridge_default_isr
app_irq_6A:
    BR !!0x050D4
bridge_irq_6C .VECTOR 0x6C
.SECTION .text, TEXT
bridge_irq_6C:
    CMP !0xF710,#0xA5
    BZ $app_irq_6C
    BR !!bridge_default_isr
app_irq_6C:
    BR !!0x050D8
bridge_irq_6E .VECTOR 0x6E
.SECTION .text, TEXT
bridge_irq_6E:
    CMP !0xF710,#0xA5
    BZ $app_irq_6E
    BR !!bridge_default_isr
app_irq_6E:
    BR !!0x050DC
bridge_irq_70 .VECTOR 0x70
.SECTION .text, TEXT
bridge_irq_70:
    CMP !0xF710,#0xA5
    BZ $app_irq_70
    BR !!bridge_default_isr
app_irq_70:
    BR !!0x050E0
bridge_irq_72 .VECTOR 0x72
.SECTION .text, TEXT
bridge_irq_72:
    CMP !0xF710,#0xA5
    BZ $app_irq_72
    BR !!bridge_default_isr
app_irq_72:
    BR !!0x050E4
bridge_irq_74 .VECTOR 0x74
.SECTION .text, TEXT
bridge_irq_74:
    CMP !0xF710,#0xA5
    BZ $app_irq_74
    BR !!bridge_default_isr
app_irq_74:
    BR !!0x050E8
bridge_irq_76 .VECTOR 0x76
.SECTION .text, TEXT
bridge_irq_76:
    CMP !0xF710,#0xA5
    BZ $app_irq_76
    BR !!bridge_default_isr
app_irq_76:
    BR !!0x050EC
bridge_irq_78 .VECTOR 0x78
.SECTION .text, TEXT
bridge_irq_78:
    CMP !0xF710,#0xA5
    BZ $app_irq_78
    BR !!bridge_default_isr
app_irq_78:
    BR !!0x050F0
bridge_irq_7A .VECTOR 0x7A
.SECTION .text, TEXT
bridge_irq_7A:
    CMP !0xF710,#0xA5
    BZ $app_irq_7A
    BR !!bridge_default_isr
app_irq_7A:
    BR !!0x050F4
bridge_irq_7C .VECTOR 0x7C
.SECTION .text, TEXT
bridge_irq_7C:
    CMP !0xF710,#0xA5
    BZ $app_irq_7C
    BR !!bridge_default_isr
app_irq_7C:
    BR !!0x050F8
bridge_default_isr:
    RETI
_F24Bridge_Jump:
    DI
    CALL !!_R_Config_UART0_Stop
    CALL !!_R_Config_TAU0_1_Stop
    MOV !0xF710,#0xA5
    BR !!0x05000
