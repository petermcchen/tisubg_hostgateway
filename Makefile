bbb_TI_PROC_SDK_DIR=/src/atheros_lsdk920_pure/build/gcc-4.3.3
bbb_BIN_DIR=${bbb_TI_PROC_SDK_DIR}/build_mips/staging_dir/usr/bin
bbb_PREFIX=${bbb_BIN_DIR}/mips-linux-uclibc-

SUBGMGR_HOME    := /src/atheros_lsdk920_pure/apps/mtlapps
MTL_INSTALL_PATH=$(SUBGMGR_HOME)/mtl_install
MTL_ADDONS_PATH=$(SUBGMGR_HOME)/mtl_addons
#MTL_HEM_PATH=$(SUBGMGR_HOME)/HEM

INC_PATH =
#INC_PATH = -I$(SUBGMGR_HOME)/src
#INC_PATH += -I$(SUBGMGR_HOME)/mtlcommon
#INC_PATH += -I$(MTL_HEM_PATH)/C_Process/include
#INC_PATH += -I$(MTL_ADDONS_PATH)/mtl_config
#INC_PATH += -I$(MTL_INSTALL_PATH)/usr/include
INC_PATH += -I$(MTL_ADDONS_PATH)/jansson/src
SHARE_LIB_PATH :=
SHARE_LIB :=
SHARE_LIB += -lpthread
SHARE_LIB_PATH += -L$(MTL_INSTALL_PATH)/usr/lib
SHARE_LIB += -ljansson
SHARE_LIB += -lmtl_config

bbb_GCC_EXE       := ${bbb_PREFIX}gcc
bbb_ARCH_CFLAGS   += -ffunction-sections
bbb_AR_EXE        := ${bbb_PREFIX}ar
bbb_OBJDUMP_EXE   := ${bbb_PREFIX}objdump
bbb_ARCH_LDFLAGS  += -Wl,--gc-sections
CFLAGS_STRICT= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS +=-Wall -Wmissing-prototypes -Wstrict-prototypes -Iinc $(INC_PATH) -xc -v -Xlinker --verbose

ROBJ = socketread.o 
WOBJ = socketwrite.o 
TOBJ = checkendian.o 
SOBJ = subgcli.o 
TGOBJ = testgetchar.o
TSOBJ = teststatic.o
ASOBJ = subgcli_a.o 
OUOBJ = oadu.o

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

testgetchar: $(TGOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

teststatic: $(TSOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

subgcli_a: $(ASOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS}

oadu: $(OUOBJ)
	$(bbb_GCC_EXE) -o $@ $^ $(CFLAGS) ${ARCH_CFLAGS} $(SHARE_LIB_PATH) $(SHARE_LIB)

.PHONY: clean print_inc

clean:
	rm -f *.o

print_inc:
	echo $(INC_PATH)
