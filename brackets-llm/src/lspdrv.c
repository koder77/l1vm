/* lspdrv.c - test harness: exercise lspclient exactly like brackets-llm does */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lspclient.h"

int main(int argc, char **argv)
{
    LspClient *c;
    LspDiagVec diags = {0};
    size_t i;
    int ck;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.l1com>\n", argv[0]);
        return 1;
    }
    c = lsp_start("/home/stefan/l1vm/bin/l1vm-lsp",
                  1, "/home/stefan/l1vm/bin/l1com", "/home/stefan/l1vm/include/");
    if (!c) {
        fprintf(stderr, "lsp_start failed\n");
        return 1;
    }
    {   /* read file */
        char uri[2048], path[2048];
        FILE *f = fopen(argv[1], "rb");
        long sz;
        char *buf;
        if (!f) {
            fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc((size_t)sz + 1);
        if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
            return 1;
        fclose(f);
        buf[sz] = '\0';
        snprintf(path, sizeof(path), "%s", argv[1]);
        snprintf(uri, sizeof(uri), "file://%s", path);
        ck = lsp_check(c, uri, path, buf, &diags);
        fprintf(stderr, "lsp_check rc=%d diags=%zu\n", ck, diags.len);
        for (i = 0; i < diags.len; i++) {
            fprintf(stderr, "  diag[%zu] sev=%d line=%d msg=%s\n",
                    i, diags.items[i].severity, diags.items[i].line,
                    diags.items[i].message ? diags.items[i].message : "(null)");
        }
        lsp_diagvec_free(&diags);
        free(buf);
    }
    lsp_stop(c);
    return 0;
}