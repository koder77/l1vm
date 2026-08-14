/*
 * l1vm-lsp - entry point
 * LSP server for the Brackets language (L1VM), JSON-RPC over stdio
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

void lsp_main_loop(void);

int main(void)
{
    lsp_main_loop();
    return 0;
}
