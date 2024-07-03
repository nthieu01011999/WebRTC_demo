include $(SRCCODE)/.vbuild/build.mk

obj-clean       := $(foreach obj,$(obj-Y) $(dir),$(obj)-clean)
obj-install     := $(foreach obj,$(obj-Y),$(obj)-install)
obj-romfs       := $(foreach obj,$(obj-Y),$(obj)-romfs)

all:$(obj-Y)

clean: $(obj-clean)

romfs: $(obj-romfs)

install: $(obj-install)

#
# Generic rules
#

%:
	[ ! -d $* ] || $(MAKE) -f make.inc -C $*
%-clean:
	[ ! -d $* ] || $(MAKE) -f make.inc -C $* clean

%-install:
	[ ! -d $* ] || $(MAKE) -f make.inc -C $* install

%-romfs:
	[ ! -d $* ] || $(MAKE) -f make.inc -C $* romfs

$(obj-Y) $(obj-N) $(obj-clean) $(obj-install): dummy

.PHONY: all clean install
