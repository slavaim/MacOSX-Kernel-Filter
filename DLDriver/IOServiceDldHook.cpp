/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "IOServiceDldHook.h"

//--------------------------------------------------------------------

//
// instantiate implicitly or else the compiler will skip a vtable creation as there is no
// obvious instantiation point for the template in the code flow,
// gcc has its idiosyncrasy in templates instantiation containing static members
// so implicit instantiation is important
//

template class IOServiceDldHook2<DldInheritanceDepth_0>;
template class IOServiceDldHook2<DldInheritanceDepth_1>;
template class IOServiceDldHook2<DldInheritanceDepth_2>;
template class IOServiceDldHook2<DldInheritanceDepth_3>;
template class IOServiceDldHook2<DldInheritanceDepth_4>;
template class IOServiceDldHook2<DldInheritanceDepth_5>;
template class IOServiceDldHook2<DldInheritanceDepth_6>;
template class IOServiceDldHook2<DldInheritanceDepth_7>;
template class IOServiceDldHook2<DldInheritanceDepth_8>;
template class IOServiceDldHook2<DldInheritanceDepth_9>;
template class IOServiceDldHook2<DldInheritanceDepth_10>;

template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_0>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_1>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_2>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_3>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_4>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_5>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_6>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_7>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_8>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_9>,IOService>;
template class DldHookerCommonClass2<IOServiceDldHook2<DldInheritanceDepth_10>,IOService>;

//--------------------------------------------------------------------
