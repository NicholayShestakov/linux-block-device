# Linux block device
Linux block devices for educational practice.  
Contains a RAM IO block device and a block device that is used over a base device.  
The devices was developed for Fedora 43 with kernel version 6.18.13-200.fc43.x86_64. 

## Build and use

### RAM IO block device
For build go to folder with device code and execute make
```
$ cd ramio-blk
$ make
```
For install RAM IO module to kernel
```
$ sudo insmod ramioblk.ko
```
For check that the device is succesfully loaded use
```
$ lsmod | grep ramioblk
```
If nothing is displayed use command below to see which step could not successfully ended
```
$ dmesg | grep ramioblk
```
If device is displayed use this for create file system and mount it
```
$ sudo mkfs.ext4 /dev/ramioblk
$ sudo mount /dev/ramioblk <path_to_mount_directory>
```
Congratulations! You are successfully created new disk under the control of a block device with data storage in <path_to_mount_directory>.  
All data created in it will be saved in RAM area which controlled by block device.  
When the device will be unloaded, all data recorded on it will be lost.  
To unload the device use
```
$ sudo umount <path_to_mount_directory>
$ sudo rmmod ramioblk
```

### Over base device block device
For build go to folder with device code and execute make
```
$ cd over-basic-device-blk
$ make
```
For install module over base device to kernel  
```
$ sudo insmod obdblk.ko base_device_name=<path_to_base_device>
```
For check that the device is succesfully loaded use
```
$ lsmod | grep obdblk
```
If nothing is displayed use command below to see which step could not successfully ended
```
$ dmesg | grep obdblk
```
If device is displayed use this for create file system and mount it
```
$ sudo mkfs.ext4 /dev/ramioblk
$ sudo mount /dev/ramioblk <path_to_mount_directory>
```
Now, if you write something to this device, it will transmit it to the base device.  
At the moment device collects statistics about bytes transmitted through it in both sides and can display it with command
```
$ cat /sys/block/obdblk/obd_statistics
```
To unload the device use
```
$ sudo umount <path_to_mount_directory>
$ sudo rmmod ramioblk
```
