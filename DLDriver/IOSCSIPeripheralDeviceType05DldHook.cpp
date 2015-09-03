/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldIOKitHookEngine.h"
#include "IOSCSIPeripheralDeviceType05DldHook.h"

//--------------------------------------------------------------------

//
// instantiate implicitly or else the compiler will skip a vtable creation as there is no
// obvious instantiation point for the template in the code flow,
// gcc has its idiosyncrasy in templates instantiation containing static members
// so implicit instantiation is important
//

//
// NMSmartplugSCSIDevice is a driver class for USB CD/DVD and it inherits from IOSCSIPeripheralDeviceType05,
// so the depth inheritance declaration is important
//
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_0>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_1>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_2>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_3>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_4>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_5>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_6>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_7>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_8>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_9>;
template class IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_10>;

template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_0>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_1>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_2>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_3>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_4>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_5>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_6>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_7>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_8>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_9>,IOSCSIPeripheralDeviceType05>;
template class DldHookerCommonClass2<IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_10>,IOSCSIPeripheralDeviceType05>;

//---------------------------------------------------------------------

//
// a header for IOSCSIPeripheralDeviceType05DldHook can't be included in other
// cpp files as there is a conflict of defenitions, to register a hooking class
// a function calling an instance of DldAddHookingClassInstance is provided here
//
bool DldHookEngine_Add_IOSCSIPeripheralDeviceType05DldHooks( __in DldIOKitHookEngine* hookEngine )
{
    //
    // the hooks are vtable hooks as the object hooks are supported only for DldInheritanceDepth_0, should the hooks be converted to object
    // hooks the object hook for NMSmartplugSCSIDevice class must be provided
    //
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_0>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_1>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_2>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_3>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_4>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_5>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_6>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_7>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_8>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_9>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );
    hookEngine->DldAddHookingClassInstance< IOSCSIPeripheralDeviceType05DldHook<DldInheritanceDepth_10>, IOSCSIPeripheralDeviceType05 >( DldHookTypeVtable );

    return true;
}

//--------------------------------------------------------------------
