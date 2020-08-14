bbb_TI_PROC_SDK_DIR=/src/atheros_lsdk920_pure/build/gcc-4.3.3
bbb_BIN_DIR=${bbb_TI_PROC_SDK_DIR}/build_mips/staging_dir/usr/bin
bbb_PREFIX=${bbb_BIN_DIR}/mips-linux-uclibc-

bbb_GCC_EXE       := ${bbb_PREFIX}gcc
bbb_ARCH_CFLAGS   += -ffunction-sections
bbb_AR_EXE        := ${bbb_PREFIX}ar
bbb_OBJDUMP_EXE   := ${bbb_PREFIX}objdump
bbb_ARCH_LDFLAGS  += -Wl,--gc-sections
CFLAGS_STRICT= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS +=-Wall -Wmissing-prototypes -Wstrict-prototypes -Iinc -xc -v -Xlinker --verbose

ROBJ = socketread.o 
WOBJ = socketwrite.o 
TOBJ = checkendian.o 
SOBJ = subgcli.o 

%.o: %.c $(DEPS)
	$(bbb_GCC_EXE) -c -o $@ $< $(CFLAGS)

socketread: $(ROBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

socketwrite: $(WOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

checkendian: $(TOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

subgcli: $(SOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

.PHONY: clean

clean:
	rm -f *.o
