/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */


#include "IOUSBMassStorageClassDldHook.h"

//--------------------------------------------------------------------

//
// instantiate implicitly or else the compiler will skip a vtable creation as there is no
// obvious instantiation point for the template in the code flow,
// gcc has its idiosyncrasy in templates instantiation containing static members
// so implicit instantiation is important
//
template class IOUSBMassStorageClassDldHook2<DldInheritanceDepth_0>;
template class DldHookerCommonClass2<IOUSBMassStorageClassDldHook2<DldInheritanceDepth_0>,IOUSBMassStorageClass>;

//--------------------------------------------------------------------

