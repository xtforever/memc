#ifndef CTX_H
#define CTX_H


int ctx_init(int *CTX,int length, int size );
void ctx_free(int *CTX, int p, void (*free_cb)(int *ctx, int p));
void ctx_destruct(int *CTX, void (*free_cb)(int *ctx, int p));

#endif
