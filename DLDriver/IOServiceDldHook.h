/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef _IOSERVICEDLDHOOK_H
#define _IOSERVICEDLDHOOK_H

#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <sys/types.h>
#include <sys/kauth.h>
#include "DldCommon.h"
#include "DldHookerCommonClass.h"
#include "DldHookerCommonClass2.h"
/*
//--------------------------------------------------------------------

//
// IOServiceVtableDldHook##Depth defines a hook for a class's vtable derived from the
// IOService through an inheritance chain of the Depth length ( i.e. the 0 depth is an IOService instance )
// the destiction between classes is required as it is impossible to infer
// when the derived class calles super::someFunction ( which is called through the super class's vtable ), 
// if the derived and super classes are hooked by the same class as a result an infinite recursion
// will occur due to the nature of the GetOriginalFunction routine
//

//--------------------------------------------------------------------

#define DldDeclareIoServiceVtableDldHook( Depth ) \
    class IOServiceVtableDldHook##Depth : public DldHookerBaseInterface\
    {\
        OSDeclareDefaultStructors( IOServiceVtableDldHook##Depth )\
        DldDeclareCommonIOServiceHookFunctionAndStructorsWithDepth( IOServiceVtableDldHook##Depth, IOService, IOService, Depth )\
\
        DldVirtualFunctionsEnumDeclarationStart( IOServiceVtableDldHook##Depth )\
        DldAddCommonVirtualFunctionsEnumDeclaration( IOServiceVtableDldHook##Depth )\
        DldVirtualFunctionsEnumDeclarationEnd( IOServiceVtableDldHook##Depth )\
\
        DldDeclarePureVirtualHelperClassStart( IOServiceVtableDldHook##Depth, IOService )\
        DldDeclarePureVirtualHelperClassEnd( IOServiceVtableDldHook##Depth, IOService )\
\
    public:\
\
    };

//
// the classes name will IOServiceVtableDldHookDldInheritanceDepth_XX (!) as for ## the name but not a value is used
//

#define IoServiceVtableDldHookClassStr( Depth )   "IOServiceVtableDldHook"##Depth
#define IoServiceVtableDldHookClass( Depth ) IOServiceVtableDldHook##Depth

DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_0 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_1 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_2 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_3 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_4 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_5 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_6 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_7 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_8 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_9 )
DldDeclareIoServiceVtableDldHook( DldInheritanceDepth_10 )


//--------------------------------------------------------------------


class IOServiceDldHook : public IOService
{
    OSDeclareDefaultStructors( IOServiceDldHook )
    DldDeclareCommonIOServiceHookFunctionAndStructors( IOServiceDldHook, IOService, IOService )
    
    
    /////////////////////////////////////////////////////////
    //
    // declaration for the hooked functions enum
    //
    /////////////////////////////////////////////////////////
    DldVirtualFunctionsEnumDeclarationStart( IOServiceDldHook )
    DldAddCommonVirtualFunctionsEnumDeclaration( IOServiceDldHook )
    DldVirtualFunctionsEnumDeclarationEnd( IOServiceDldHook )
    
    ////////////////////////////////////////////////////////
    //
    // a helper virtual class declaration
    //
    /////////////////////////////////////////////////////////
    DldDeclarePureVirtualHelperClassStart( IOServiceDldHook, IOService )
    DldDeclarePureVirtualHelperClassEnd( IOServiceDldHook, IOService )
    
public:
    
    ////////////////////////////////////////////////////////
    //
    // hooking function declarations
    //
    /////////////////////////////////////////////////////////
    
    
    ////////////////////////////////////////////////////////////
    //
    // end of hooking function decarations
    //
    //////////////////////////////////////////////////////////////
    
};
*/
//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
class IOServiceDldHook2 : public DldHookerBaseInterface
{
    
    friend class DldIOKitHookEngine;
    friend class DldHookerCommonClass2<IOServiceDldHook2<Depth>,IOService>;
    
public:
    
    enum{
        kDld_NumberOfAddedHooks = 0x0
    };
    
    static const char* fGetHookedClassName(){ return "IOService"; };
    DldDeclareGetClassNameFunction();
    
protected:
    static IOServiceDldHook2<Depth>* newWithDefaultSettings();
    virtual bool init();
    virtual void free();
    
protected:
    
    static DldInheritanceDepth fGetInheritanceDepth(){ return Depth; };
    DldHookerCommonClass2< IOServiceDldHook2<Depth> ,IOService >*  mHookerCommon2;
};

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
IOServiceDldHook2<Depth>*
IOServiceDldHook2<Depth>::newWithDefaultSettings()
{
    IOServiceDldHook2<Depth>*  newObject;
    
    newObject = new IOServiceDldHook2<Depth>();
    assert( newObject );
    if( !newObject )
        return NULL;
    
    newObject->mHookerCommon2 = new DldHookerCommonClass2< IOServiceDldHook2<Depth> ,IOService >();
    assert( newObject->mHookerCommon2 );
    if( !newObject->mHookerCommon2 ){
        
        delete newObject;
        return NULL;
    }
    
    newObject->init();
    
    return newObject;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
bool IOServiceDldHook2<Depth>::init()
{
    //super::init() no super to init
    
    if( !this->mHookerCommon2->init( this ) ){
        
        DBG_PRINT_ERROR(("this->mHookerCommon2.init( this ) failed\n"));
        return false;
    }
    
    return true;
}

//--------------------------------------------------------------------

template<DldInheritanceDepth Depth>
void IOServiceDldHook2<Depth>::free()
{
    this->mHookerCommon2->free();
    
    //super::free() - no super to free
}

//--------------------------------------------------------------------

#endif//_IOSERVICEDLDHOOK_H

