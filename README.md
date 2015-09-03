# MacKernelFilter

The project contains a kernel mode driver for Mac OS X , aka kernel extension ( kext ).  
The driver allows to intercept requests to any internal or external device ( USB, FireWire, PCI, Thunderbolt, Bluetooth, WiFi ), allow or disable requests based on user credentials and device type, perform collection of data sent to external storage devices.  
The driver allows to change data read or written from/to external storages.  
The driver allows to intercept network trafic, collect it in the user mode, change it and reinject in the network.  
The driver can protect itself and selected user mode processes even from users with root privilege ( to a certain degree, depending on attackers skills ).  
The driver doesn't require to be loaded on boot, the special technique used in the driver allows it to be loaded anytime and control devices started before it was loaded.

You can use the code and compiled driver for educational purposses. The commercial usage or code/binary distribution is not allowed.  

The driver was developed in 2009-2013 and tested on Mac OS X Snow Leopard, Mac OS X Lion, Mac OS X Mountain Lion, Mac OS X Mavericks, Mac OS X Yosemite.  
The project requires XCode 5.

You can contact the author via his blog http://modpager.blogspot.com.au or facebook https://www.facebook.com/profile.php?id=100010119092130 or linkedin https://www.linkedin.com/profile/view?id=6575657  

Slava Imameev  
Sydney  
September 2015  
