# resizing Bela

## situation

- installed bela image on SD card

## problem

- 32GB SD card
- 4GB partition
- 1.5GB free space

## solution

all of this assumes root on a linux with SD card reader

replace `/dev/sdX` with SD card device

WARNING: using wrong device in some steps will cause catastrophic data loss

### backup host machine

use whichever method you like

backup partition table of all disks, for example

```
sfdisk -d /dev/sda > sda
```

make a copy outside the machine!

then physically unplug any backup disks

### backup sd card

take up to about 4GB space with default bela image

```
sfdisk -d /dev/sdX > sdX
cat /dev/sdX1 | gzip -9 > sdX1.gz
cat /dev/sdX2 | gzip -9 > sdX2.gz
```

### resize partition

inspect free space

```
sfdisk -F /dev/sdX
```

note down end sector of free space at end of partition

```
cp sdX sdX.new
nano sdX.new
```

change size of the second partition to
the end sector of free space minus
the start sector of the second partition

don't change anything else

### write new partition table

WARNING: using wrong device in this step will cause catastrophic data loss

```
sfdisk /dev/sdX < sdX.new
```

### resize filesystem

```
resize2fs /dev/sdX
sync
```

### check it worked

```
mkdir foo/
mount -o ro /dev/sdX foo/
df -h foo/
umount foo/
```

## recovery

if something went wrong you can restore SD card from backup

WARNING: using wrong device in this step will cause catastrophic data loss

```
sfdisk /dev/sdX < sdX
zcat sdX1.gz > /dev/sdX1
zcat sdX2.gz > /dev/sdX2
```
