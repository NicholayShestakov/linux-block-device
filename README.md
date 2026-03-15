# Linux block device
Linux block device for educational practice.  
At the moment, the repository provides the simplest block device with functions for reading and writing to RAM.  
The device was developed for Fedora 43 with kernel version 6.18.13-200.fc43.x86_64. 

## Build and use
For build execute
```
$ make
```
For install module to kernel
```
$ sudo insmod myblk.ko
```
For check that the device is succesfully loaded use
```
$ lsmod | grep myblk
```
If nothing is displayed use command below to see which step could not successfully ended
```
$ dmesg | grep myblk
```
If device is displayed use this for create file system and mount it
```
$ sudo mkfs.ext4 /dev/myblk
$ sudo mount /dev/myblk <path_to_mount_directory>
```
Congratulations! You are successfully created new disk under the control of a block device with data storage in <path_to_mount_directory>.  
All data created in it will be saved in RAM area which controlled by block device.  
When the device will be unloaded, all data recorded on it will be lost.  
To unload the device use
```
$ sudo umount <path_to_mount_directory>
$ sudo rmmod myblk
```
