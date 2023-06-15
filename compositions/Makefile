all: template.zip

%.zip: %/ %/*
	rm -f $@
	zip -9 -r $@ $<
