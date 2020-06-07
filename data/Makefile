# ls /Volumes/World\ of\ Tanks/res/packages/*.pkg | sed -n 's/.*\/\([0-9][0-9]*_.*\).pkg$/maps\/images\/\1.png/p' | xargs
WOT_INSTALL_PATH ?= /media/windows/Games/World_of_Tanks
MAPS = $(shell ls $(WOT_INSTALL_PATH)/res/packages/*.pkg | sed -n 's@.*/\([0-9][0-9]*_.*\).pkg$$@maps/images/\1.png  maps/definitions/\1.xml@p')
SIZE ?= 512

.PRECIOUS: maps/definitions/%.xml map/images/%.png

default: minimaps

all: minimaps

clean:
	@rm -rf maps
	@rm -f convert-xml

%.pkg:
	@cp "$(WOT_INSTALL_PATH)/res/packages/$@" ./

spaces/%/mmap.dds: %.pkg
	@mkdir -p maps/images
	@unzip -o $< spaces/$(basename $<)/mmap.dds > /dev/null
	@rm -f $<

maps/definitions/%.xml: convert-xml
	@mkdir -p maps/definitions
	@unzip -p "$(WOT_INSTALL_PATH)/res/packages/scripts.pkg" "scripts/arena_defs/$(notdir $@)" | \
		./convert-xml $(notdir $@) > $@

maps/images/%.png: spaces/%/mmap.dds
	@convert -size $(SIZE)x$(SIZE) $< -alpha on -resize $(SIZE)x$(SIZE) $@
	@echo Generated $@
	@rm -f $<

minimaps: $(MAPS)
	@rm -rf gui spaces

convert-xml:
	@mono-csc -out:convert-xml ../ext/wottools/src/*.cs