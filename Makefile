LDFLAGS=-lhiredis



all:sip_worker 

sip_worker:main.c 
	$(CC) -o $@ $< $(LDFLAGS) `pkg-config --cflags --libs libpjproject libevent json-c `

clean:
	rm -f myapp.o myapp
