# Paper Title:
Designing a distributed data store over bare-metal Raspberry Pi.

### Abstract
This paper presents a primitive distributed filesystem, implemented over
a collection of bare metal raspberry pi's. By programming in bare-metal,
we as developers achieve extreme programmatic control over the actions of
the pi as well as much more granular access to the underlying disk data.
The project extends the FAT32 disk system of each pi into a globally
syncrhonized data store, and demonstrates that a scalable and reliable 
storage system can be implemented over a large number of inexpensive
commodity storage hardware components.

The design of this filesystem is motivated by the fact that it is useful
to be able to coordinate the storage capabilities of multiple physical
disks. Given access to a decent number of medium-sized storage devices
(sd cards, flash drives, external disks, etc.), it would be useful to 
coordinate data storage over all of them via a single filesystem interface
as opposed to doing the manual coordination of data yourself. From migrating
your photo library from the cloud, to downloading large datasets for 
off-line access, to supporting data-intensive applications without an
internet connection, the list of use cases is extensive.

This filesystem effectively demonstrates that the implementation of such
a system is possible, and useful. Furthermore, this paper explores how
existing filesystems, like the fat32 system, can be extended to support a 
distributed store and still maintain backwards compatibility with existing 
software (like the booting pi firmware).

This paper discusses the explorations made by this project.



To run what's there, connect one or more pies and run
```sh
./build/mac/deploy ./build/pi/pi-side.bin
```

System primitives:
- sd_readblock(...) defined in pi/bzt-sd.h
- sd_writeblock(...) "
^^ Use these to interpret the existing fat32 filesystem,
write a dead simple (stateful) sector allocator that we can use
to maintain sectors in use by our system. We will use the page
allocator to read and write sectors for our file system.

pi-mac interface:
mac-side:
- To send data from the mac to pi n (where n indicates the 
index into the pi-install.c:line 55:pies array), we use the 
write system call on pies[n].fd where fd is the pies file
descriptor. Read works the same way.
pi-side:
- to send data from a pi to the mac, we issue uart commands:
uart_put8(char c) sends a character to the mac, to be read when
the mac issues a read system call on the pi's associated fd. 
uart_get8() returns the latest byte that the mac wrote to the
pi's fd. uart_hex puts a 32 bit int into the pi->mac wire
in little-end order.




Todo:
- Install macfuse (https://osxfuse.github.io)
- Go through each method contained in \
		/usr/local/include/fuse/fuse.h::struct fuse_operations: \
	Determine what each function is supposed to do in a regular filesystem. \
	Understand the sfsro equivalent. \
	Implement.


open:
opendir:
readdir:
releasedir:

init:
destroy:
access??:

read_buf:

For any FUSE filesystem, include this in implementation file:

#define FUSE_USE_VERSION 26

#include "fuse.h"

