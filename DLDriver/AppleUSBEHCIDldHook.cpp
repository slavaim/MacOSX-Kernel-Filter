/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */


#include "AppleUSBEHCIDldHook.h"

//
// gcc has its idiosyncrasy in templates instantiation containing static members
// so implicit instantiation is important
//

//
// EHCI controller hook
//
template class AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeEHCI>;
template class DldHookerCommonClass2<AppleUSBHCIDldHook<DldInheritanceDepth_0,kDldUsbHciTypeEHCI>,IOUSBControllerV3>;

//
// OHCI controller hook
//
template class AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeOHCI>;
template class DldHookerCommonClass2<AppleUSBHCIDldHook<DldInheritanceDepth_0,kDldUsbHciTypeOHCI>,IOUSBControllerV3>;

//
// UHCI controller hook
//
template class AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeUHCI>;
template class DldHookerCommonClass2<AppleUSBHCIDldHook<DldInheritanceDepth_0,kDldUsbHciTypeUHCI>,IOUSBControllerV3>;

//
// XHCI controller hook
//
template class AppleUSBHCIDldHook< DldInheritanceDepth_0, kDldUsbHciTypeXHCI>;
template class DldHookerCommonClass2<AppleUSBHCIDldHook<DldInheritanceDepth_0,kDldUsbHciTypeXHCI>,IOUSBControllerV3>;