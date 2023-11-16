DIRS = $(shell ls -d */)
ZIPS = $(patsubst %/,%.zip,$(DIRS))

zip: $(ZIPS)

%.zip: %/ %/* Makefile
	rm -rf $@ $</build/
	zip -9 -r $@ $<
