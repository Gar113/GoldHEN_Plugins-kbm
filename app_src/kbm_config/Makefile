# KBM Config — aplikasi homebrew PS4 (PKG)
# Mengubah mapping keyboard untuk plugin kbm.prx

TITLE       := KBM Config
VERSION     := 1.00
# TITLE_ID memakai format resmi homebrew: BREW + 5 digit angka.
# Format ini yang dipakai semua sample OpenOrbis yang terbukti bisa
# diinstall di PS4. TITLE_ID dengan huruf (mis. KBMCFG001) ditolak
# oleh installer PS4 walau PkgTool tetap menerimanya saat build.
TITLE_ID    := BREW00101
CONTENT_ID  := IV0000-BREW00101_00-KBMCONFIG0000000

# Library yang di-link
LIBS        := -lc -lkernel -lm -lc++ -lSceVideoOut -lSceSysmodule -lSceFreeType -lScePad -lSceKeyboard -lSceUserService -lSceAudioOut

# Butuh FreeType untuk menggambar teks
EXTRAFLAGS  := -DGRAPHICS_USES_FONT

ASSETS      := $(wildcard assets/**/*)
LIBMODULES  := $(wildcard sce_module/*)

# Root vars
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := source
INTDIR      := build

CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.o, $(CFILES)) $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.o, $(CPPFILES))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

_unused     := $(shell mkdir -p $(INTDIR))

UNAME_S     := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CC      := clang
	CCX     := clang++
	LD      := ld.lld
	CDIR    := linux
	PKGENV  := DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
	# PkgTool.Core (ditulis pakai .NET Core 3.0) butuh OpenSSL 1.1.
	# Kalau sistem hanya punya OpenSSL 3, arahkan ke salinan lokal
	# OpenSSL 1.1 tanpa perlu install ke sistem.
	OSSL11  := $(shell [ -d /tmp/opencode/ossl11/usr/lib ] && echo /tmp/opencode/ossl11/usr/lib || true)
	ifeq ($(OSSL11),)
		OSSL11 := $(shell ls -d /opt/openssl-1.1/lib 2>/dev/null || true)
	endif
	ifneq ($(OSSL11),)
		PKGENV  := LD_LIBRARY_PATH=$(OSSL11):$$LD_LIBRARY_PATH $(PKGENV)
	endif
endif
ifeq ($(UNAME_S),Darwin)
	CC      := /usr/local/opt/llvm/bin/clang
	CCX     := /usr/local/opt/llvm/bin/clang++
	LD      := /usr/local/opt/llvm/bin/ld.lld
	CDIR    := macos
	PKGENV  := DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
endif

all: $(CONTENT_ID).pkg

# ---- PKG ----
$(CONTENT_ID).pkg: pkg.gp4
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGENV) $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

# ---- binary ----
eboot.bin: $(INTDIR) $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/kbm_config.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/kbm_config.elf -out=$(INTDIR)/kbm_config.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(PROJDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR):
	mkdir -p $(INTDIR)

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 eboot.bin \
		$(INTDIR)/kbm_config.elf $(INTDIR)/kbm_config.oelf $(OBJS)

.PHONY: all clean
