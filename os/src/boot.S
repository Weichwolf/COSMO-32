# COSMO-32 Boot Code
# Erster Code der nach Reset ausgeführt wird

.section .text.init
.global _start

_start:
    # Stack initialisieren
    la      sp, _stack_top

    # BSS nullen
    la      t0, _bss_start
    la      t1, _bss_end
1:  bge     t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    # .data von Flash nach SRAM kopieren
    la      t0, _data_load
    la      t1, _data_start
    la      t2, _data_end
3:  bge     t1, t2, 4f
    lw      t3, 0(t0)
    sw      t3, 0(t1)
    addi    t0, t0, 4
    addi    t1, t1, 4
    j       3b
4:

    # Main aufrufen
    call    main

    # Endlosschleife falls main zurückkehrt
halt:
    wfi
    j       halt
