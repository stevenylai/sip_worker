#can't find pkg-config for hiredis 
LDFLAGS=-lhiredis

all:sip_worker 

sip_worker:main.c 
	$(CC) -o $@ $< $(LDFLAGS) -g `pkg-config --cflags --libs libpjproject libevent json-c libconfig `

clean:
	rm -f sip_worker.o sip_worker
