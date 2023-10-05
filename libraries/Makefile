default:
	@echo "run make install to copy to bela.local"

backup:
	ssh root@bela.local tar --create --verbose --bzip --file "/root/Bela-libraries-REBUS-$(shell date --iso=s).tar.bz2" /root/Bela/libraries/REBUS

install:
	ssh root@bela.local rm -rf /root/Bela/libraries/REBUS
	scp -r REBUS root@bela.local:/root/Bela/libraries/
