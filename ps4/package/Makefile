# Packages PSNokia (the PS4 build of phoneME MIDP) with one game. Run from
# a staging directory prepared by make_game.sh, which passes TITLE,
# TITLE_ID and CONTENT_ID.
#
#   /app0/game.jar    - the MIDlet to run (its manifest names the class)
#   /app0/lib         - Chameleon LCDUI skin resources
#   /app0/appdb       - seed for MIDP_HOME/appdb (/data/psnokia/<TITLE_ID>)
#   /app0/titleid.txt - TITLE_ID, read at startup (sce_sys is not readable)
#   /app0/title.txt   - the game's name, for the menu
#   /app0/screen.txt  - optional phone screen size, "WxH"
#   /app0/assets      - the menu's font (with its licence) and game icon

VERSION     := 1.00
# The release or debug build, as chosen in env.sh (PSNOKIA_BUILD)
MIDP_OUT    := $(PSNOKIA_MIDP_OUT)
MIDP_ELF    := $(MIDP_OUT)/bin/c/$(PSNOKIA_MIDP_ELF)
TOOLS       := $(OO_PS4_TOOLCHAIN)/bin/windows
# System modules and the about file, as shipped with the OpenOrbis samples
OO_SAMPLE   := $(OO_PS4_TOOLCHAIN)/samples/audio-wav

LIBMODULES  := sce_module/libc.prx sce_module/libSceFios2.prx
LIBFILES    := $(addprefix lib/,$(notdir $(wildcard $(MIDP_OUT)/lib/*)))
APPDBFILES  := $(addprefix appdb/,$(notdir $(wildcard $(MIDP_OUT)/appdb/*)))
FONTFILES   := assets/fonts/Gontserrat-Regular.ttf assets/fonts/OFL.txt

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLS)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png game.jar titleid.txt title.txt $(wildcard screen.txt) $(LIBFILES) $(APPDBFILES) $(LIBMODULES) $(FONTFILES) $(wildcard assets/icon.raw)
	$(TOOLS)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"
	# create-gp4 only declares its own fixed directories; add ours
	sed -i 's|<dir targ_name="sce_module" />|<dir targ_name="sce_module" /><dir targ_name="lib" /><dir targ_name="appdb" />|' $@

titleid.txt:
	printf '%s' '$(TITLE_ID)' > $@

title.txt:
	printf '%s' '$(TITLE)' > $@

assets/fonts/%: $(PSNOKIA_PS4)/package/assets/fonts/%
	@mkdir -p assets/fonts
	cp $< $@

sce_module/%: $(OO_SAMPLE)/sce_module/%
	@mkdir -p sce_module
	cp $< $@

sce_sys/about/right.sprx: $(OO_SAMPLE)/sce_sys/about/right.sprx
	@mkdir -p sce_sys/about
	cp $< $@

lib/%: $(MIDP_OUT)/lib/%
	@mkdir -p lib
	cp $< $@

appdb/%: $(MIDP_OUT)/appdb/%
	@mkdir -p appdb
	cp $< $@

sce_sys/param.sfo:
	@mkdir -p sce_sys
	$(TOOLS)/PkgTool.Core sfo_new $@
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(MIDP_ELF)
	$(TOOLS)/create-fself -in=$< -out=psnokia.oelf --eboot "eboot.bin" --paid 0x3800000000000011
