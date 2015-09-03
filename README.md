# MacKernelFilter

The project contains a kernel mode driver for Mac OS X , aka kernel extension ( kext ).  
The driver allows to intercept requests to any internal or external device ( USB, FireWire, PCI, Thunderbolt ), allow or disable requests based on user credentials and device type, perform data collection send to external storage devices.  
The driver allows to change data read or written from/to external devices.  
The driver allows to intercept network trafic, collect it in the user mode, change it and reinject in the network.  

You can use the driver for educational purposses. The commercial usage or code/binary distribution is not allowed.  
The driver was developed in 2009-2011 and tested up to Mac OS X Mavericks.

Slava Imameev  
Sydney  
September 2015  
