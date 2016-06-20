# MacOSX-Kernel-Filter

The license model is a BSD Open Source License. This is a non-viral license, only asking that if you use it, you acknowledge the authors, in this case Slava Imameev.

The project contains a kernel mode driver filter for Mac OS X , aka kernel extension ( kext ).  
The driver allows to intercept requests to any internal or external device ( USB, FireWire, PCI, Thunderbolt, Bluetooth, WiFi ), allow or disable requests based on user credentials and device type, perform collection of data sent to external storage devices.  
The driver allows to change data read or written from/to external storages.  
The driver allows to intercept network trafic, collect it in the user mode, change it and reinject in the network.  
The driver can protect itself and selected user mode processes even from users with root privilege ( to a certain degree, depending on attackers skills ).  
The driver doesn't require to be loaded on boot, the special technique used in the driver allows it to be loaded anytime and control devices started before it was loaded.

The driver was developed in 2009-2013 and tested on Mac OS X Snow Leopard, Mac OS X Lion, Mac OS X Mountain Lion, Mac OS X Mavericks, Mac OS X Yosemite.  
The project requires XCode 5.

The driver uses a hooking technique to provide filtering functionality. The driver hooks C++ virtual table for IOKit classes to filter access to IOKit objects and file operation tables for VFS to filter access to file systems.

Apple I/O Kit is a set of classes to develop kernel modules for Mac OS X and iOS. Its analog in the Windows world is KMDF/UMDF framework. I/O Kit is built atop Mach and BSD subsystems like Windows KMDF is built atop WDM and kernel API.  
  
The official way to develop a kernel module filter for Mac OS X and iOS is to inherit filter C++ class from a C++ class it filters. This requires access to a class declaration which is not always possible as some classes are Apple private or not published by a third party developers. In most cases all these private classes are inherited from classes that are in a public domain. That means that they extend an existing interface/functionality and a goal to filter device I/O can be achieved by filtering only the public interface. This is nearly always true because an I/O Kit class object that is attached to this private C++ class object usually an Apple I/O Kit class that knows nothing about the third party extended interface or is supposed to be from a module developed by third party developers. That means that in both cases the attached object issues requests to a public interface.  
  
Let's consider some imaginary private class IOPrivateInterface that inherits from some IOAppleDeviceClass which declaration is available and an attached I/O Kit object issues requests to IOAppleDeviceClass interface  
  
class IOPrivateInterface: public IOAppleDeviceClass {   
};  
  
You want to filter requests to a device managed by IOPrivateInterface, that means that you need to declare your filter like  
  
class IOMyFilter: public IOPrivateInterface{  
};  
  
this would never compile as you do not have access to IOPrivateInterface class. You can't declare you filter as  

class IOMyFilter: public IOAppleDeviceClass {
};  

as this will jettison all IOPrivateInterface code and the device will not function correctly.  

There might be another reason to avoid standard I/O Kit filtering by inheritance. A filtering class objects replaces an original class object in the I/O Kit device stack. That means a module with a filter should be available on device discovery and initialization. In nearly all cases this means that an instance of a filter class object will be created during system startup. This puts a great responsibility on a module developer as an error might render the system unbootable without an easy solution for a customer to fix the problem.  
  
To overcome this limitation I developed a hooking technique for I/O kit classes. I/O Kit uses virtual C++ functions so a class can be extended but its clients still be able to use a base class declaration. That means that all functions that used for I/O are placed in the array of function pointers known as vtable.  
  
The hooking technique supports two types of hooking.
  - replacing vtable array pointer in class object  
  - replacing selected functions in vtable array without changing vtable pointer  
  
The former method allows to filter access to a particular object of a class but requires knowing a vtable size. The latter method allows to filter request without knowing vtable size but as vtable is shared by all objects of a class a filter will see requests to all objects of a particular class. To get a vtable size you need a class declaration or get the size by reverse engineering.  
  
The hooker code can be found in DldHookerCommonClass.cpp .   
  
The driver uses C++ templates to avoid code duplication. Though Apple documentation declares that C++ templates are not supported by I/O Kit it is true only if an I/O Kit object is used as a template parameter. You can compile I/O kit module with C++ template classes if an I/O Kit class pointer or non I/O kit derived class is used as a template parameter. As you probably know after instantiation a template is just an ordinary C++ class. Template classes support is not required from run time environment. You can't use I/O Kit as a template parameter just because a way Apple declares them by using C style preprocessor definitions.  
  
To load driver run the following commands, in my case the project's directory is /work/DL/GitHub/DLDriver  
  
 $ sudo chown -R root:wheel /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriver.kext  
 $ sudo chown -R root:wheel /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriverPrivate.kext  
 $ sudo kextutil -v /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriver.kext -d /work/DL/GitHub/DLDriver/DLDriver/Build/Products/Release/DLDriverPrivate.kext  
 
 To verify that the driver has loaded run  
  $ kextstat | grep SlavaImameev  
  
  the output should be like  
  
  132    1 0xffffff8030ae5400 0xd0       0xd0       SlavaImameev.devicelock.agent.privatedriver (1.0)  
  133    0 0xffffff7f82265000 0xd8000    0xd8000    SlavaImameev.devicelock.agent.driver (1.0) <132 118 7 5 4 3 2 1>  

Slava Imameev  
Sydney  
September 2015  
