#!/bin/sh
cbmc main-scheduler.c load-object.c debugger.c ../lib-func/string.c \
--pointer-check --bounds-check --signed-overflow-check \
--pointer-overflow-check --div-by-zero-check \
--unwind 2 --object-bits 16 --slice-formula \
--no-unwinding-assertions > cbmc.txt 2>&1

# Filtert jetzt gezielt nach FAILED Checks, besonders in den String-Funktionen
echo "--- Alle fehlgeschlagenen Prüfungen (FAILED) ---"
grep "FAILURE" cbmc.txt

echo "\n--- Spezifische Analyse der String-Funktionen ---"
grep -E "string_func|strlen_safe|string\.c" cbmc.txt | grep "FAILURE"
