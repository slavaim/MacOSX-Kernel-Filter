# MacOSX-Kernel-Filter

The project contains a kernel mode driver filter for Mac OS X , aka kernel extension ( kext ).  
The driver allows to intercept requests to any internal or external device ( USB, FireWire, PCI, Thunderbolt, Bluetooth, WiFi ), allow or disable requests based on user credentials and device type, perform collection of data sent to external storage devices.  
The driver allows to change data read or written from/to external storages.  
The driver allows to intercept network trafic, collect it in the user mode, change it and reinject in the network.  
The driver can protect itself and selected user mode processes even from users with root privilege ( to a certain degree, depending on attackers skills ).  
The driver doesn't require to be loaded on boot, the special technique used in the driver allows it to be loaded anytime and control devices started before it was loaded.

You can use the code and compiled driver for educational purposses. The commercial usage or code/binary distribution is not allowed.  

The driver was developed in 2009-2013 and tested on Mac OS X Snow Leopard, Mac OS X Lion, Mac OS X Mountain Lion, Mac OS X Mavericks, Mac OS X Yosemite.  
The project requires XCode 5.

The driver uses a hooking technique to provide filtering functionality. The driver hooks C++ virtual table for IOKit classes to filter access to IOKit objects and file operation tables for VFS to filter access to file systems.

To load driver run the following commands, in my case the project's directory is /work/DL/GitHub/DLDriver

 $ sudo chown -R root:wheel /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriver.kext  
 $ sudo chown -R root:wheel /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriverPrivate.kext  
 $ sudo kextutil -v /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriver.kext -d /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriverPrivate.kext  
 
 To verify that the driver has loaded run  
  $ kextstat | grep SlavaImameev  
  
  the output should be like  
  
  132    1 0xffffff8030ae5400 0xd0       0xd0       SlavaImameev.devicelock.agent.privatedriver (1.0)  
  133    0 0xffffff7f82265000 0xd8000    0xd8000    SlavaImameev.devicelock.agent.driver (1.0) <132 118 7 5 4 3 2 1>  

Feel free to contact the author if you have any question. You can contact the author via his blog http://modpager.blogspot.com.au or facebook https://www.facebook.com/profile.php?id=100010119092130 or linkedin https://www.linkedin.com/profile/view?id=6575657  

Slava Imameev  
Sydney  
September 2015  
