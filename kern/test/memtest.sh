gcc -m32 -O -fno-builtin -g -Wall -DMTSAFE=1 -DMAGNEWLK=1 -DHACKS=1 -DSLABHUGELK=0 -DMAGLK=0 -DNEWLK=1 -DMAGBITMAP=1 -DMEMTEST=1 -I../.. -I../../usr/lib -o memtest memtest.c ../mem/mag.c ../mem/slab.c -pthread
