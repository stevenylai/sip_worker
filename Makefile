SIP_VERSION = 2.4
PACKAGE_INSTALL_DIR = ${PWD}/build/install
SIP_DL = build/dl/pjproject-$(SIP_VERSION).tar.bz2
SIP_DIR = build/sip
SIP_WORK_DIR = $(SIP_DIR)/pjproject-$(SIP_VERSION)
SIP_HEADER = $(PACKAGE_INSTALL_DIR)/include/pjsua-lib/pjsua.h
SIP_LIB = $(PACKAGE_INSTALL_DIR)/lib/pkgconfig/libpjproject.pc
SIP_TARGET = $(SIP_HEADER) $(SIP_LIB)
SIP_DEP =

all:sip_worker

sip-all: $(SIP_DEP) $(SIP_TARGET)
$(SIP_TARGET): $(SIP_DL)
	mkdir -p $(SIP_DIR) $(PACKAGE_INSTALL_DIR)
	bzcat $(SIP_DL) | tar xvf - -C $(SIP_DIR) > /dev/null
	find $(SIP_WORK_DIR) -name '*.py'|xargs rm -f
	cd $(SIP_WORK_DIR) && CFLAGS="-g -fPIC" ./configure --prefix=$(PACKAGE_INSTALL_DIR) --host=$(host_alias)
	cd $(SIP_WORK_DIR) && $(MAKE)
	cd $(SIP_WORK_DIR) && $(MAKE) install

$(SIP_DL):
	mkdir -p build/dl
	wget -O - http://www.pjsip.org/release/$(SIP_VERSION)/pjproject-$(SIP_VERSION).tar.bz2 > $@

sip-clean:
	if [ -d $(SIP_WORK_DIR) ]; then \
		cd $(SIP_WORK_DIR) && $(MAKE) uninstall; \
		cd $(SIP_WORK_DIR) && $(MAKE) clean; \
		rm -Rf $(SIP_TARGET); \
	fi

sip_worker: sip-all main.c 
	$(CC) -o $@ -g main.c $(LDFLAGS) \
	     -lhiredis \
	     `pkg-config --cflags --libs libevent json-c libconfig $(PACKAGE_INSTALL_DIR)/lib/pkgconfig/libpjproject.pc`

clean: sip-clean
	rm -f sip_worker.o sip_worker
