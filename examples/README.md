# how to deploy

1. make a backup of the sd card just in case
   (in particular, the `/root/Bela/examples/REBUS` folder if it exists,
   the next step will delete it and replace with the current version)
2. `make install`
3. first time only,
   `ssh root@bela.local nano /root/Bela/examples/order.json`
   and add REBUS to the list
4. first time only,
   reboot the board to reload the examples in the IDE
