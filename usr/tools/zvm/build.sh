#! /bin/sh

clang -g -DZAS32BIT=1 -DZVM=1 -DZVMVIRTMEM=0 -O -I../../lib -I../.. -I.. -o zvm zvm.c mem.c op.c asm.c sig.c ../zas/zas.c
