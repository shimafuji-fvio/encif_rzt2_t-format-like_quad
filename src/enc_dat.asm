.section ".rodata"
.balign 32
.global g_enc_config_dat
g_enc_config_dat:
.incbin "./gpencif8b.dat"
.balign 32
.global g_pinmux_config
g_pinmux_config:
.incbin "../lib/ecl/RZT2_pinmux_gpenc8b_1ch_full.bin"
.end
