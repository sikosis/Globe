INSTALL_DIR ?= /boot/home/config/non-packaged/bin

.PHONY: all install clean

all:
	$(MAKE) -C src

install: all
	$(MAKE) -C src install INSTALL_DIR="$(INSTALL_DIR)"

clean:
	$(MAKE) -C src clean
