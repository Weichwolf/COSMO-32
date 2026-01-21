# COSMO-32 Main
# Initialisiert System und startet Shell

.section .text
.global main

main:
    # Shell initialisieren
    call    shell_init

    # Display initialisieren
    call    display_init

    # Shell starten (kehrt nie zurück)
    call    shell_run

    # Falls doch: Endlosschleife
1:  wfi
    j       1b
