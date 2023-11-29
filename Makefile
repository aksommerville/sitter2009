ifeq ($(MAKECMDGOALS),clean)
  clean:;@rm -rf bin linuxsdl linuxdrm wii win32
else

SRCDIR:=src
SRCDATADIR:=$(SRCDIR)/data
SRCEXTRADIR:=$(SRCDIR)/extra
BINDIR:=bin
BINDATADIR:=$(BINDIR)/data
DATARULES:=$(BINDIR)/data.mk
PKGDIR:=pkg

CXXFLAGS_all:=-c -MMD -O2

SEGMENTED_IMAGES:=bg/bricks.png bg/dakota.png bg/dangersign.png bg/faint_pillar.png bg/flags.png \
                  bg/goal.png bg/goalsign.png bg/monstersign.png bg/moonbase.png bg/short-tuffet.png \
                  bg/sporksign.png bg/tree.png bg/tuffet.png
SEGMENTED_IMAGES:=$(addprefix $(SRCDATADIR)/default/graphics/,$(SEGMENTED_IMAGES))

IMAGE_CONVERTER:=$(BINDIR)/imgcvt
IMAGE_CONVERTER_SOURCE:=$(SRCDIR)/sitter_image_converter.cpp

#ARCHS:=linux wii win32
ARCHS:=linuxsdl linuxdrm
all:processed_data_files $(addprefix arch_,$(ARCHS))

###############################################################################
# architecture targets

# linux, with libsdl
DST_linuxsdl:=linuxsdl
APP_linuxsdl:=$(DST_linuxsdl)/sitter
DATADIR_linuxsdl:=$(DST_linuxsdl)/data
CXX_linuxsdl:=g++
CXXFLAGS_linuxsdl:=$(CXXFLAGS_all) -DSITTER_LINUX -DSITTER_LITTLE_ENDIAN \
                -DSITTER_CFG_PREARGV=\":~/.sitter/sitter.cfg\" \
                -DSITTER_CFG_DEST=\"~/.sitter/sitter.cfg\" \
                -DSITTER_DATA_DIR=\"$(abspath $(DATADIR_linuxsdl))/\" \
                -DSITTER_HIGHSCORE_FILE=\"~/.sitter/sitter.highscore\"
LDFLAGS_linuxsdl:=
LDLIBS_linuxsdl:=-lSDL -lGL -lz
APPEXTRA_linuxsdl:=LICENSE README INSTALL
$(DATADIR_linuxsdl)/%:$(BINDATADIR)/%|all_data_dirs;@cp $< $@

# linux, with libdrm, alsa, and evdev, for systems with no window manager
DST_linuxdrm:=linuxdrm
APP_linuxdrm:=$(DST_linuxdrm)/sitter
DATADIR_linuxdrm:=$(DST_linuxdrm)/data
CXX_linuxdrm:=g++
CXXFLAGS_linuxdrm:=$(CXXFLAGS_all) -DSITTER_LINUX_DRM -DSITTER_LITTLE_ENDIAN \
                -DSITTER_CFG_PREARGV=\":~/.sitter/sitter.cfg\" \
                -DSITTER_CFG_DEST=\"~/.sitter/sitter.cfg\" \
                -DSITTER_DATA_DIR=\"$(abspath $(DATADIR_linuxdrm))/\" \
                -DSITTER_HIGHSCORE_FILE=\"~/.sitter/sitter.highscore\" \
                -I/usr/include/libdrm
LDFLAGS_linuxdrm:=
LDLIBS_linuxdrm:=-ldrm -lEGL -lgbm -lGL -lasound -lpthread -lz
APPEXTRA_linuxdrm:=LICENSE README INSTALL
$(DATADIR_linuxdrm)/%:$(BINDATADIR)/%|all_data_dirs;@cp $< $@

# windows. only useful if you're building from linux with GCC for i586-mingw32msvc (ubuntu provides this)
DST_win32:=win32
APP_win32:=$(DST_win32)/sitter.exe
DATADIR_win32:=$(DST_win32)/data
CXX_win32:=i586-mingw32msvc-g++
CXXFLAGS_win32:=$(CXXFLAGS_all) -DSITTER_WIN32 -DSITTER_LITTLE_ENDIAN \
                -DSITTER_CFG_PREARGV=\":sitter.cfg\" \
                -DSITTER_CFG_DEST=\"sitter.cfg\" \
                -DSITTER_DATA_DIR=\"data/\" \
                -DSITTER_HIGHSCORE_FILE=\"sitter.highscore\"
LDFLAGS_win32:=
LDLIBS_win32:=-lSDL -lopengl32 -lz
APPEXTRA_win32:=SDL.dll zlib1.dll LICENSE README
$(DATADIR_win32)/%:$(BINDATADIR)/%|all_data_dirs;@cp $< $@

# wii. the paths to devkitpro and libogc are for my system (i have a few different versions, and can't decide on one...)
WII_DKP_PREFIX:=/home/yrr892/wii/devkitPro/devkitPPC-r17/bin/powerpc-gekko-
WII_OGC_INCLUDE:=-I/home/yrr892/wii/libogc-src/gc
WII_OGC_INCBIN:=-L/home/yrr892/wii/libogc-src/lib/wii
WII_FAT_INCLUDE:=-I/home/yrr892/wii/devkitPro/libfat/include
WII_FAT_INCBIN:=-L/home/yrr892/wii/devkitPro/libfat/libogc/lib/wii
WII_SD_PREFIX:=sd0:
DST_wii:=wii
APP_wii:=$(DST_wii)/boot.elf
DATADIR_wii:=$(DST_wii)/data
CXX_wii:=$(WII_DKP_PREFIX)g++
CXXFLAGS_wii:=$(CXXFLAGS_all) -mrvl -meabi -mcpu=750 -mhard-float -DGEKKO $(WII_OGC_INCLUDE) $(WII_FAT_INCLUDE) \
              -DSITTER_WII -DSITTER_BIG_ENDIAN \
              -DSITTER_CFG_PREARGV=\"+$(WII_SD_PREFIX)/apps/sitter/sitter.cfg\" \
              -DSITTER_CFG_DEST=\"$(WII_SD_PREFIX)/apps/sitter/sitter.cfg\" \
              -DSITTER_DATA_DIR=\"$(WII_SD_PREFIX)/apps/sitter/data/\" \
              -DSITTER_HIGHSCORE_FILE=\"$(WII_SD_PREFIX)/apps/sitter/sitter.highscore\"
LDFLAGS_wii:=-mrvl -meabi -mcpu=750 -mhard-float $(WII_OGC_INCBIN) $(WII_FAT_INCBIN)
LDLIBS_wii:=-lfat -lwiiuse -lbte -logc -lz
APPEXTRA_wii:=meta.xml icon.png LICENSE README
$(DATADIR_wii)/%.png:$(BINDATADIR)/%.png $(IMAGE_CONVERTER)|all_data_dirs;@$(IMAGE_CONVERTER) -out=$@ -togx -cvt -no-compress $<
$(DATADIR_wii)/%:$(BINDATADIR)/%|all_data_dirs;@cp $< $@

###############################################################################
# read source files

SRCFILES:=$(shell find $(SRCDIR) -type f)
SRCDATAFILES:=$(filter $(SRCDATADIR)/%,$(SRCFILES))
SRCEXTRAFILES:=$(filter $(SRCDATADIR)/%,$(SRCFILES))
SRCCODEFILES:=$(filter-out $(SRCDATAFILES) $(SRCEXTRAFILES) $(IMAGE_CONVERTER_SOURCE),$(SRCFILES))
SRCHDRFILES:=$(filter %.h,$(SRCCODEFILES))
SRCCODEFILES:=$(filter %.cpp,$(SRCCODEFILES))
SRCINCLUDE:=$(sort $(addprefix -I,$(dir $(SRCHDRFILES))))
SRCDIRS:=$(patsubst %/,%,$(sort $(dir $(SRCCODEFILES))))
SRCDATADIRS:=$(patsubst %/,%,$(sort $(dir $(SRCDATAFILES))))
SRCEXTRADIRS:=$(patsubst %/,%,$(sort $(dir $(SRCEXTRAFILES))))
MKDIRS:=$(sort $(patsubst $(SRCDIR)%,$(BINDIR)%,$(SRCDIRS)))

###############################################################################
# data files

UNSEGMENTED_DATA:=$(filter-out $(SEGMENTED_IMAGES),$(SRCDATAFILES))
$(DATARULES):$(SRCDATAFILES) $(IMAGE_CONVERTER);@echo $(DATARULES) ; \
  rm -f $(DATARULES) ; \
  echo "DATAFILES_MID:=" >> $(DATARULES) ; \
  for f in $(SEGMENTED_IMAGES) ; do \
    PFX="$$(echo $$f | sed 's/\(.*\)\.png/\1/')" ; \
    OUTPFX="$$(echo $$PFX | sed 's,^$(SRCDATADIR),$(BINDATADIR),')" ; \
    $(IMAGE_CONVERTER) -enum $$f | ( read COLC ROWC ; \
      OUTS="" ; \
      for x in `seq 0 $$((COLC-1))` ; do \
        for y in `seq 0 $$((ROWC-1))` ; do \
          OUTS="$$OUTPFX-$$y$$x.png $$OUTS" ; \
        done ; \
      done ; \
      D=$$(dirname $$OUTPFX) ; \
      echo "DATA_MKDIRS+=$$D" >> $(DATARULES) ; \
      echo "$$OUTS:$$f $(IMAGE_CONVERTER)|$$D;@$(IMAGE_CONVERTER) -seg -samefmt -out=$$OUTPFX $$f" >> $(DATARULES) ; \
      echo "processed_data_files:$$OUTS" >> $(DATARULES) ; \
      echo "DATAFILES_MID+=$$OUTS" >> $(DATARULES) ; \
    ) ; \
  done ; \
  for f in $(UNSEGMENTED_DATA) ; do \
    OUT="$$(echo $$f | sed 's,^$(SRCDATADIR)\(.*\)$$,$(BINDATADIR)\1,')" ; \
    D=$$(dirname $$OUT) ; \
    echo "DATA_MKDIRS+=$$D" >> $(DATARULES) ; \
    echo "$$OUT:$$f|$$D;@cp $$f $$OUT" >> $(DATARULES) ; \
    echo "processed_data_files:$$OUT" >> $(DATARULES) ; \
    echo "DATAFILES_MID+=$$OUT" >> $(DATARULES) ; \
  done ; \
  for arch in $(ARCHS) ; do \
    echo "DATAFILES_$$arch:="'$$(patsubst $(BINDATADIR)%,$$(DATADIR_'$$arch')%,$$(DATAFILES_MID))' >> $(DATARULES) ; \
    echo 'DATA_MKDIRS+=$$(sort $$(patsubst %/,%,$$(dir $$(DATAFILES_'$$arch'))))' >> $(DATARULES) ; \
  done ; \
  echo 'DATA_MKDIRS:=$$(sort $$(DATA_MKDIRS))' >> $(DATARULES) ; \
  echo 'DATA_MKDIRS_OUT:=' >> $(DATARULES) ; \
  echo 'define DATA_MKDIR_RULES' >> $(DATARULES) ; \
  echo '  P:=$$(patsubst %/,%,$$(dir $$1))' >> $(DATARULES) ; \
  echo '  $$1:|$$$$P;@mkdir $$$$@' >> $(DATARULES) ; \
  echo '  DATA_MKDIRS_OUT+=$$1' >> $(DATARULES) ; \
  echo '  $$$$(if $$$$(filter $$$$P,$$(DATA_MKDIRS_OUT)),,$$$$(if $$$$(filter . $(BINDIR) $(ARCHS),$$$$P),,$$$$(eval $$$$(call DATA_MKDIR_RULES,$$$$P))))' >> $(DATARULES) ; \
  echo 'endef' >> $(DATARULES) ; \
  echo '$$(foreach F,$$(filter-out .,$$(DATA_MKDIRS)),$$(eval $$(call DATA_MKDIR_RULES,$$F)))' >> $(DATARULES) ; \
  echo 'all_data_dirs:$$(DATA_MKDIRS_OUT)' >> $(DATARULES)

-include $(DATARULES)
  
###############################################################################
# per-architecture rules

define ARCH_RULES # $1=arch
  ifeq ($1,linuxdrm)
    OBJFILES_$1:=$(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.$1.o,$(SRCCODEFILES))
  else
    OBJFILES_$1:=$(patsubst $(SRCDIR)/%.cpp,$(BINDIR)/%.$1.o,$(filter-out src/drm_alsa_evdev/%,$(SRCCODEFILES)))
  endif
  $(APP_$1):$$(OBJFILES_$1)|$(dir $(APP_$1));@echo $(APP_$1) ; \
    $(CXX_$1) $(LDFLAGS_$1) -o $(APP_$1) $$(OBJFILES_$1) $(LDLIBS_$1)
  $(BINDIR)/%.$1.o:$(SRCDIR)/%.cpp|bindirs;@echo $$@ ; \
    $(CXX_$1) $(CXXFLAGS_$1) $(SRCINCLUDE) -o $$@ $$<
  -include $$(OBJFILES_$1:.o=.d)
  arch_$1:$(APP_$1) $$(DATAFILES_$1) $(addprefix $(DST_$1)/,$(APPEXTRA_$1))
  $(DST_$1)/%:$(SRCEXTRADIR)/%;@cp $$< $$@
  MKDIRS+=$(patsubst %/,%,$(dir $(APP_$1)))
endef

$(foreach A,$(ARCHS),$(eval $(call ARCH_RULES,$A)))
  
###############################################################################
# image converter

IMAGE_CONVERTER_OBJ:=$(patsubst $(SRCDIR)%.cpp,$(BINDIR)%.o,$(IMAGE_CONVERTER_SOURCE))
ifneq (,$(strip $(filter linuxsdl,$(ARCHS))))
  SITTEROBJ:=$(filter-out $(BINDIR)/sitter_main.linuxsdl.o,$(OBJFILES_linuxsdl))
else ifneq (,$(strip $(filter linuxdrm,$(ARCHS)))
  SITTEROBJ:=$(filter-out $(BINDIR)/sitter_main.linuxdrm.o,$(OBJFILES_linuxdrm))
else
  $(error Please update Makefile. Need some app code in imgcvt, must call out architecture)
endif
$(BINDIR)/%.o:$(SRCDIR)/%.cpp|$(BINDIR);@echo $@ ; g++ -c -MMD -O2 $(SRCINCLUDE) -o $@ $< -DSITTER_LITTLE_ENDIAN
$(IMAGE_CONVERTER):$(IMAGE_CONVERTER_OBJ) $(SITTEROBJ) |$(dir $(IMAGE_CONVERTER));@echo $(IMAGE_CONVERTER) ; \
  g++ -o $(IMAGE_CONVERTER) $(IMAGE_CONVERTER_OBJ) $(SITTEROBJ) -lSDL -lGL -lz
-include $(IMAGE_CONVERTER_OBJ:.o=.d)

###############################################################################
# directories

MKDIRS_OUT:=
define MKDIR_RULES # $1=dir
  P:=$(patsubst %/,%,$(dir $1))
  $1:|$$P;@mkdir $$@
  MKDIRS_OUT+=$1
  $$(if $$(filter $$P,$$(MKDIRS_OUT)),,$$(if $$(filter .,$$P),,$$(eval $$(call MKDIR_RULES,$$P))))
endef
$(foreach F,$(filter-out .,$(MKDIRS)),$(eval $(call MKDIR_RULES,$F)))
bindirs:$(MKDIRS_OUT)

###############################################################################
# dist

DIST_TAG:=$(shell date +'%Y%m%d')
dist:arch_linuxsdl arch_linuxdrm arch_wii arch_win32;@ \
  if [ -d $(PKGDIR)/$(DIST_TAG) ] ; then \
    TAG=$(DIST_TAG)-$$(date +'%s') ; \
  else TAG=$(DIST_TAG) ; fi ; \
  mkdir $(PKGDIR)/$$TAG || exit 1 ; \
  \
  UNIT="$(PKGDIR)/$$TAG/sitter-src-$$TAG" ; \
  mkdir $$UNIT || exit 1 ; \
  cp -r $(SRCDIR) Makefile $(SRCEXTRADIR)/LICENSE $(SRCEXTRADIR)/README $$UNIT || exit 1 ; \
  tar -cjf $$UNIT.tar.bz2 $$UNIT && rm -r $$UNIT || exit 1 ; \
  \
  UNIT="$(PKGDIR)/$$TAG/sitter-linuxsdl-amd64-$$TAG" ; \
  cp -r $(DST_linuxsdl) $$UNIT || exit 1; \
  tar -cjf $$UNIT.tar.bz2 $$UNIT && rm -r $$UNIT || exit 1 ; \
  \
  UNIT="$(PKGDIR)/$$TAG/sitter-linuxdrm-$$TAG" ; \
  cp -r $(DST_linuxdrm) $$UNIT || exit 1; \
  tar -cjf $$UNIT.tar.bz2 $$UNIT && rm -r $$UNIT || exit 1 ; \
  \
  UNIT="$(PKGDIR)/$$TAG/sitter-win32-$$TAG" ; \
  cp -r $(DST_win32) $$UNIT || exit 1; \
  zip -rq $$UNIT.zip $$UNIT && rm -r $$UNIT || exit 1 ; \
  \
  UNIT="$(PKGDIR)/$$TAG/sitter-wii-$$TAG" ; \
  cp -r $(DST_wii) $$UNIT || exit 1; \
  tar -cjf $$UNIT.tar.bz2 $$UNIT && rm -r $$UNIT || exit 1 ; \

endif
