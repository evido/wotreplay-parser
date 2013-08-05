WOT_INSTALL_PATH ?= /media/windows/Games/World_of_Tanks
MAPS = maps/00_tank_tutorial.png maps/85_winter.png maps/60_asia_miao.png maps/73_asia_korea.png maps/01_karelia.png maps/02_malinovka.png maps/03_campania.png maps/04_himmelsdorf.png maps/05_prohorovka.png maps/06_ensk.png maps/07_lakeville.png maps/08_ruinberg.png maps/10_hills.png maps/11_murovanka.png maps/13_erlenberg.png maps/14_siegfried_line.png maps/15_komarin.png maps/17_munchen.png maps/18_cliff.png maps/19_monastery.png maps/22_slough.png maps/23_westfeld.png maps/28_desert.png maps/29_el_hallouf.png maps/31_airfield.png maps/33_fjord.png maps/34_redshire.png maps/35_steppes.png maps/36_fishing_bay.png maps/37_caucasus.png maps/38_mannerheim_line.png maps/39_crimea.png maps/42_north_america.png maps/44_north_america.png maps/45_north_america.png maps/47_canada_a.png maps/51_asia.png

default: minimaps

all: minimaps

clean:
	@rm -rf maps

%.pkg:
	@cp $(WOT_INSTALL_PATH)/res/packages/$@ ./

spaces/%/mmap.dds: %.pkg
	@mkdir -p maps
	@unzip -o $< spaces/$(basename $<)/mmap.dds > /dev/null
	@rm -f $<

maps/%.png: spaces/%/mmap.dds
	@convert -size 512x512 $< -alpha on -resize 512x512 $@
	@echo Generated $@
	@rm -f $<

minimaps: $(MAPS)
	@rm -rf gui

