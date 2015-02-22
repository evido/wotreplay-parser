# ls /Volumes/World\ of\ Tanks/res/packages/*.pkg | sed -n 's/.*\/\([0-9][0-9]*_.*\).pkg$/maps\/images\/\1.png/p' | xargs
WOT_INSTALL_PATH ?= /media/windows/Games/World_of_Tanks
MAPS = maps/images/00_tank_tutorial.png maps/images/01_karelia.png maps/images/02_malinovka.png maps/images/03_campania.png maps/images/04_himmelsdorf.png maps/images/05_prohorovka.png maps/images/06_ensk.png maps/images/07_lakeville.png maps/images/08_ruinberg.png maps/images/100_thepit.png maps/images/103_ruinberg_winter.png maps/images/10_hills.png maps/images/11_murovanka.png maps/images/13_erlenberg.png maps/images/14_siegfried_line.png maps/images/15_komarin.png maps/images/17_munchen.png maps/images/18_cliff.png maps/images/19_monastery.png maps/images/22_slough.png maps/images/23_westfeld.png maps/images/28_desert.png maps/images/29_el_hallouf.png maps/images/31_airfield.png maps/images/33_fjord.png maps/images/34_redshire.png maps/images/35_steppes.png maps/images/36_fishing_bay.png maps/images/37_caucasus.png maps/images/38_mannerheim_line.png maps/images/39_crimea.png maps/images/43_north_america.png maps/images/44_north_america.png maps/images/45_north_america.png maps/images/47_canada_a.png maps/images/53_japan.png maps/images/60_asia_miao.png maps/images/63_tundra.png maps/images/73_asia_korea.png maps/images/83_kharkiv.png maps/images/84_winter.png maps/images/86_himmelsdorf_winter.png maps/images/92_stalingrad.png maps/images/95_lost_city.png maps/images/96_prohorovka_defense.png

default: minimaps

all: minimaps

clean:
	@rm -rf maps

%.pkg:
	@cp "$(WOT_INSTALL_PATH)/res/packages/$@" ./

spaces/%/mmap.dds: %.pkg
	@mkdir -p maps/images
	@unzip -o $< spaces/$(basename $<)/mmap.dds > /dev/null
	@rm -f $<

maps/images/%.png: spaces/%/mmap.dds
	@convert -size 512x512 $< -alpha on -resize 512x512 $@
	@echo Generated $@
	@rm -f $<

minimaps: $(MAPS)
	@rm -rf gui

