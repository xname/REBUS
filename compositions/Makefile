all: template.zip

%.zip: %/ %/* Makefile
	rm -rf $@ $</build/
	zip -9 -r $@ $<
