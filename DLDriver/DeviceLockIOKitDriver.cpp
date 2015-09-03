/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/assert.h>
#include <IOKit/IOCatalogue.h>
#include "DeviceLockIOKitDriver.h"
#include "DldUndocumentedQuirks.h"
#include "IOUSBMassStorageClassDldHook.h"
#include "DldIORegistryEntry.h"
#include "DldMachO.h"
#include "DldVNodeHook.h"
#include "IOBSDSystem.h"
#include "DldVnodeHashTable.h"
#include "DldIOUserClient.h"
#include "DldIOLog.h"
#include "DldKernAuthorization.h"
#include "DldIOShadow.h"
#include "DldKauthCredArray.h"
#include "DldSCSITask.h"
#include "DldWhiteList.h"
#include "DldHookerCommonClass2.h"
#include "DldPowerManagement.h"
#include "DldDVDWhiteList.h"
#include "DldCoveringVnode.h"
#include "DldSparseFile.h"
#include "DldDiskCAWL.h"
#include "DldVfsMntHook.h"
#include "DldServiceProtection.h"
#include "DldKernelToUserEvents.h"
#include "DldIPCUserClient.h"
#include "./NKE/DldSocketFilter.h"

//--------------------------------------------------------------------

//
// a place to declare all sensitive global variables for the driver,
// just to have a common view for all global variables, the global
// variables which aere not visible outside file where they are
// declared as static are normally not placed here
//

DldDriverProperties         gDriverProperties;
SInt32                      gLogIndx = 0x0;
IORegistryPlane*            gDldDeviceTreePlan = NULL;
const char*                 gDldDeviceTreePlanName = "DldDeviceTreePlane";
DldIOKitKAuthVnodeGate*     gVnodeGate;
DldIOKitHookEngine*         gHookEngine;
DldIORegistryEntry*         gDldRootEntry;
DldIOLog*                   gLog;
DldIOShadow*                gShadow;
#ifdef _DLD_MACOSX_CAWL
DldDiskCAWL*                gDiskCAWL;
#endif // _DLD_MACOSX_CAWL
DldTypePermissionsArray*    gArrayOfAclArrays[ kDldAclTypeMax ];
DldKernelSecuritySettings   gSecuritySettings;
DldKauthCredArray*          gCredsArray;
DldWhiteList*               gWhiteList;
DldInternalLog*             gInternalLog;// can be NULL!
DldDVDWhiteList*            gCDDVDWhiteList;
DldGlobalSettings           gGlobalSettings;
DldSocketFilter*            gSocketFilter;
DldServiceProtection*       gServiceProtection;
DldIOUserClientRef          gServiceUserClient;
DldKernelToUserEvents       gKerneToUserEvents;
bool                        gFirstScanCompleted = false;
vfs_context_t               gVfsContextSuser;
Boolean                     gClientWasAbnormallyTerminated = false;

//--------------------------------------------------------------------

DldDbgData   gDldDbgData;

//--------------------------------------------------------------------

IOReturn
DldCreateRegistryRoot();

//--------------------------------------------------------------------

//
// the standard IOKit declarations
//
#define super IOService

OSDefineMetaClassAndStructors(com_devicelock_driver_DeviceLockIOKitDriver, IOService)

//--------------------------------------------------------------------

bool
com_devicelock_driver_DeviceLockIOKitDriver::start(
    __in IOService *provider
    )
{
    bool            bRet = true;
	thread_t        thread = THREAD_NULL;
	kern_return_t   result = KERN_FAILURE;
    
    //
    // set driver properties reported to the service
    //
    gDriverProperties.size = sizeof( gDriverProperties );
    gDriverProperties.interfaceVersion = DldDriverInterfaceVersionCurrent;
    
    //
    // get the root context
    //
    gVfsContextSuser = vfs_context_current();
    assert( gVfsContextSuser );
    
    //__asm__ volatile( "int $0x3" );
    //DldKdpRegister(); this was a test
    
    this->processKmodParameters();
    
    //
    // initialize the global variables
    //
    gGlobalSettings.millisecondsForLogToCatchUp = 50;
    gGlobalSettings.millisecondsWaitForFreeSpaceInNoificationsBuffer = 100;
    gGlobalSettings.maxAttemptsToResendBuffer = 2;
    
    //
    // TEST !!
    //
    // gGlobalSettings.doNotHookVFS = true;
    
    gDldDbgData.com_devicelock_driver_DeviceLockIOKitDriver = (void*)this;
    gDldDbgData.gArrayOfAclArrays = (void*)gArrayOfAclArrays;
    gDldDbgData.gGlobalSettings = (void*)&gGlobalSettings;
    
    DldPowerManagementStart( (void*)this );
    
    //
    // start internal logging
    //
    if( gGlobalSettings.logFilePath ){
        
        gInternalLog = DldInternalLog::withFileName( gGlobalSettings.logFilePath->getCStringNoCopy() );
        assert( gInternalLog );
        if( !gInternalLog ){
            
            DBG_PRINT_ERROR_TO_ASL(("gInternalLog creation failed, the kernel log will be used\n"));
        }
        
    }// end if( gGlobalSettings.logFilePath )
    
    gDldDbgData.gInternalLog = (void*)gInternalLog;
    
    if ( !super::start( provider ) ){
        
        DBG_PRINT_ERROR( ( "super::start failed" ) );
        return false;
    }
    
    if( !DldInitUndocumentedQuirks() ){
        
        assert( !"DldInitUndocumentedQuirks() failed" );
        DBG_PRINT_ERROR(("DldInitUndocumentedQuirks() failed\n"));
        return false;
    }
    
    if( kIOReturnSuccess != DldSparseFile::sInitSparseFileSubsystem() ){
        
        DBG_PRINT_ERROR( ( "DldSparseFile::sInitSparseFileSubsystem() failed\n" ) );
        return false;
    }
    
    if( !DldInitBSD() ){
        
        DBG_PRINT_ERROR( ( "DldInitBSD() failed\n" ) );
        return false;
    }
    
    if( !DldIOVnode::InitVnodeSubsystem() ){
        
        DBG_PRINT_ERROR( ( "DldIOVnode::InitVnodeSubsystem() failed\n" ) );
        return false;
    }
    
#ifdef _DLD_MACOSX_CAWL
    if( !DldCoveringFsd::InitCoveringFsd() ){
        
        DBG_PRINT_ERROR( ( "DldCoveringFsd::InitCoveringFsd() failed\n" ) );
        return false;
    }
#endif//#ifdef _DLD_MACOSX_CAWL
    
    if( !DldIPCUserClient::InitIPCUserClientStaticPart() ){
        DBG_PRINT_ERROR( ( "DldIPCUserClient::InitIPCUserClientStaticPart() failed\n" ) );
        return false;
    }
    
    //
    // kick off the service registration ( we need this to kick of com_devicelock_driver_DeviceLockIPCDriver instantiation )
    //
    this->registerService();
    
    //testGlobal.member = provider;
    //DldTestHook2();
#if defined( DBG )
    //__asm__ volatile( "int $0x3" );
#endif
    
    DldInitNotificationEvent( &this->initializationCompletionEvent );
    
    //
    // start the thread to gather information, reference the object to avoid its premature deletion
    //
    this->retain();
    
	result = kernel_thread_start ( ( thread_continue_t ) &com_devicelock_driver_DeviceLockIOKitDriver::GetDeviceTreeContinuation,
                                   this,
                                   &thread );
	assert( KERN_SUCCESS == result );
    if ( KERN_SUCCESS != result ) {
        
        DBG_PRINT_ERROR(("kernel_thread_start() failed, error=0x%x\n", result));
        this->release();
        bRet = false;
        
        goto __exit;
    }
    
    gDldDbgData.spawnThread1 = (void*)thread;
    
    //
    // wait for an initialization in a kernel thread
    //
    DldWaitForNotificationEvent( &this->initializationCompletionEvent );
    bRet = this->initializationWasSuccessful;
    
    //
    // release the thread object
    //
    thread_deallocate( thread );
    thread = THREAD_NULL;
    
__exit:
    assert( true == bRet );
    return bRet;
}

//--------------------------------------------------------------------

void
com_devicelock_driver_DeviceLockIOKitDriver::stop(
    __in IOService * provider
    )
{
    super::stop( provider );
}

//--------------------------------------------------------------------

bool com_devicelock_driver_DeviceLockIOKitDriver::init()
{
    return super::init();
}

//--------------------------------------------------------------------

//
// actually this will not be called as the module is unloadable,
// so I discontinued its support and module unload ability
//
void com_devicelock_driver_DeviceLockIOKitDriver::free()
{
    
    DBG_PRINT( ( "free() is called\n" ) );
    
    //
    // PM is the first as its handler uses the global
    // objects and might be called asynchronously
    //
    DldPowerManagementStop();
    
    if( gServiceProtection ){
        
        gServiceProtection->release();
        gServiceProtection = NULL;
    }
    
    if( gSocketFilter ){
        
        gSocketFilter->release();
        gSocketFilter = NULL;
    }
    
    if( gVnodeGate ){
        
        gVnodeGate->release();
        gVnodeGate = NULL;
    }
    
    if( gHookEngine ){
        
        gHookEngine->release();
        gHookEngine = NULL;
    }
    
    if( gCredsArray ){
        
        gCredsArray->release();// the flush is done automatically in free()
        gCredsArray = NULL;
    }
    
    if( gShadow ){
        
        gShadow->release();
        gShadow = NULL;
    }
    
    if( gLog ){
        
        gLog->release();
        gLog = NULL;
    }
    
    if( gWhiteList ){
        
        gWhiteList->release();
        gWhiteList = NULL;
    }
    
    if( gCDDVDWhiteList ){
        
        gCDDVDWhiteList->release();
        gCDDVDWhiteList = NULL;
    }
    
    DldRegistryEntriesHashTable::DeleteStaticTable();
    
    DldVnodeHooksHashTable::DeleteStaticTable();
    
    DldVnodeHashTable::DeleteStaticTable();
    
#ifdef _DLD_MACOSX_CAWL
    DldSparseFilesHashTable::DeleteStaticTable();
#endif // #ifdef _DLD_MACOSX_CAWL
    
    DldUnRegisterFakeFsd();
    
    DldSCSITask::DldFreeSCSITaskSubsystem();
    
    DldUninitBSD();    
    
    DldDeleteArrayOfAclArrays();
    
    if( gInternalLog ){
        
        gInternalLog->release();
        gInternalLog = NULL;
    }
    
    DldVfsMntHook::DestructVfsMntHook();
    
    DldSparseFile::sFreeSparseFileSubsystem();
    
    DldFreeUndocumentedQuirks();
    
    DldIOVnode::UninitVnodeSubsystem();
    
    if( gGlobalSettings.logFilePath )
        gGlobalSettings.logFilePath->release();
        
    super::free();
}

//--------------------------------------------------------------------

bool
com_devicelock_driver_DeviceLockIOKitDriver::getBoolByNameFromDict(
    __in OSDictionary*    dictionairy,
    __in const char*   name
    )
{
    
    assert( OSDynamicCast( OSDictionary, dictionairy ) );
    
    OSObject*  obj;
    
    obj = dictionairy->getObject( name );
    assert(!( obj && !OSDynamicCast( OSBoolean, obj ) ) );
    
    if( obj && OSDynamicCast( OSBoolean, obj ) )
        return OSDynamicCast( OSBoolean, obj )->isTrue();
    else
        return false;
}

//--------------------------------------------------------------------

IOReturn
com_devicelock_driver_DeviceLockIOKitDriver::fillLogFlags(
    __inout DldLogFlags*  logFlags,
    __in OSDictionary*    logFlagDictionairy
    )
{
    assert( OSDynamicCast( OSDictionary, logFlagDictionairy ) );
    
    logFlags->COMMON            = this->getBoolByNameFromDict( logFlagDictionairy, kCOMMON );
    logFlags->QUIRKS            = this->getBoolByNameFromDict( logFlagDictionairy, kQUIRKS );
    logFlags->SCSI              = this->getBoolByNameFromDict( logFlagDictionairy, kSCSI );
    logFlags->DVD_WHITE_LIST    = this->getBoolByNameFromDict( logFlagDictionairy, kDVD_WHITE_LIST );
    logFlags->WHITE_LIST        = this->getBoolByNameFromDict( logFlagDictionairy, kWHITE_LIST );
    logFlags->ACL_EVALUATUION   = this->getBoolByNameFromDict( logFlagDictionairy, kACL_EVALUATUION );
    logFlags->ACCESS_DENIED     = this->getBoolByNameFromDict( logFlagDictionairy, kACCESS_DENIED );
    logFlags->FILE_VAULT        = this->getBoolByNameFromDict( logFlagDictionairy, kFILE_VAULT );
    logFlags->POWER_MANAGEMENT  = this->getBoolByNameFromDict( logFlagDictionairy, kPOWER_MANAGEMENT );
    logFlags->DEVICE_ACCESS     = this->getBoolByNameFromDict( logFlagDictionairy, kDEVICE_ACCESS );
    logFlags->NET_PACKET_FLOW   = this->getBoolByNameFromDict( logFlagDictionairy, kNET_PACKET_FLOW );

    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

IOReturn
com_devicelock_driver_DeviceLockIOKitDriver::processKmodParameters()
{
    //
    // all errors must be printed to ASL as our proprietary log has not been initialized yet
    //
    
    OSDictionary*  rootDict;
    
    //
    // get the dictionairy wich contains all parameters
    //
    rootDict = OSDynamicCast( OSDictionary, this->getProperty( kDldParameters ) );
    assert( rootDict );
    if( !rootDict ){
        
        DBG_PRINT_ERROR_TO_ASL(("rootDict was not found"));
        return kIOReturnSuccess;
    }
    
    //
    // get a log file path
    //
    {
        OSString*   logFilePath;
        
        logFilePath = OSDynamicCast( OSString, rootDict->getObject( kLogFilePath ) );
        assert( logFilePath );
        if( logFilePath ){
            
            gGlobalSettings.logFilePath = OSString::withString( logFilePath );
            assert( gGlobalSettings.logFilePath );
            
        } else {
            
            DBG_PRINT_ERROR_TO_ASL(("OSString::withString( logFilePath ) failed"));
        }
    }
    
    gGlobalSettings.logErrors = this->getBoolByNameFromDict( rootDict, kLogErrors );

    
    //
    // fill in the log flags structure
    //
    {
        OSDictionary* logFlagDictionairy;
        
        logFlagDictionairy = OSDynamicCast( OSDictionary, rootDict->getObject( kLogFlags ) );
        assert( logFlagDictionairy );
        if( logFlagDictionairy )
            this->fillLogFlags( &gGlobalSettings.logFlags, logFlagDictionairy );
        else
            DBG_PRINT_ERROR_TO_ASL(("logFlagDictionairy was not found"));
    }
    
    return kIOReturnSuccess;
    
}

//--------------------------------------------------------------------

void
com_devicelock_driver_DeviceLockIOKitDriver::GetDeviceTreeContinuation(
    void* this_class
    )
/*
 the kernel thread function,
 this_class is an object of the com_devicelock_driver_DeviceLockIOKitDriver type,
 the object has been referenced and will be dereferenced by this function
 */
{
    assert( preemption_enabled() );
    
    com_devicelock_driver_DeviceLockIOKitDriver    *_this = NULL;
    
    DldSetDefaultSecuritySettings();
    
    //__asm__ volatile( "int $0x3" );
    
    /*
    DldModInfo   modInfo = { 0x0 };
    
    modInfo.address = (vm_map_offset_t)&_mh_execute_header;
    pmap_pte_addr = DldRetrieveModuleGlobalSymbolAddress( &modInfo, "pmap_pte" );
     */
    //pmap_pte( NULL, 0x0 );
    
    DBG_PRINT( ( "GetDeviceTreeContinuation() is called\n" ) );
    
    if( kIOReturnSuccess != DldAllocateArrayOfAclArrays() ){
        
        assert( !"DldAllocateArrayOfAclArrays failed\n" );
        DBG_PRINT_ERROR(("DldAllocateArrayOfAclArrays failed\n"));
        goto __exit_on_error;
    }
        
    DBG_PRINT(("DldAllocateArrayOfAclArrays() returned success\n"));
    
    _this = (com_devicelock_driver_DeviceLockIOKitDriver*)this_class;
    
    
    gDldDeviceTreePlan = (IORegistryPlane*)IORegistryEntry::makePlane( gDldDeviceTreePlanName );
    assert( gDldDeviceTreePlan );
    if( !gDldDeviceTreePlan ){
        
        DBG_PRINT_ERROR( ( "IORegistryEntry::makePlane( %s ) failed\n", gDldDeviceTreePlanName) );
        
        goto __exit_on_error;
    }
    
    gDldDbgData.gDldDeviceTreePlan = gDldDeviceTreePlan;
    
    DBG_PRINT(("IORegistryEntry::makePlane( %s ) returned success\n", gDldDeviceTreePlanName));
    
    //
    // init the mount's VFS hooks
    //
    if( kIOReturnSuccess != DldVfsMntHook::CreateVfsMntHook() ){
        
        assert( !"DldVfsMntHook::CreateVfsMntHook() failed" );
        DBG_PRINT_ERROR( ( "DldVfsMntHook::CreateVfsMntHook() failed\n" ) );
        
        goto __exit_on_error;
    }
    
    //
    // init the vnode subsystem, as no any kauth callback has been set the subsystem won't be called,
    // the subsystem can't be called before the vnode hooks initializtion as it calls the vnode hooks
    // subsystem when being called from the kauth callback
    //
    if( !DldVnodeHashTable::CreateStaticTable() ){
        
        assert( !"DldVnodeHashTable::CreateStaticTable() failed" );
        DBG_PRINT_ERROR( ( "DldVnodeHashTable::CreateStaticTable() failed\n" ) );
        
        goto __exit_on_error;
    }
    
    DBG_PRINT(("DldVnodeHashTable::CreateStaticTable() returned success\n"));
    
    //
    // init vnode hooks, the vnode hooks is a subsystem required for the vnode subsytem functionality
    //
    if( !DldVnodeHooksHashTable::CreateStaticTableWithSize( 8, true ) ){
        
        assert( !"DldVnodeHooksHashTable::CreateStaticTableWithSize( 20, true ) failed" );
        DBG_PRINT_ERROR( ( "DldVnodeHooksHashTable::CreateStaticTableWithSize( 20, true ) failed\n" ) );
        
        goto __exit_on_error;
    }
    
#ifdef _DLD_MACOSX_CAWL
    //
    // init sparse file hash table, the hash table is required for CAWL subsystem
    //
    if( !DldSparseFilesHashTable::CreateStaticTableWithSize( 100, true ) ){
        
        assert( !"DldSparseFilesHashTable::CreateStaticTableWithSize( 100, true ) failed" );
        DBG_PRINT_ERROR( ( "DldSparseFilesHashTable::CreateStaticTableWithSize( 100, true ) failed\n" ) );
        
        goto __exit_on_error;
    }
#endif // #ifdef _DLD_MACOSX_CAWL
    
    DBG_PRINT(("DldVnodeHooksHashTable::CreateStaticTableWithSize( 8, true ) returned success\n"));
    
    if( !DldRegistryEntriesHashTable::CreateStaticTableWithSize( 100, false ) ){
        
        assert( !"DldRegistryEntriesHashTable::CreateStaticTableWithSize( 100, false ) failed" );
        DBG_PRINT_ERROR( ( "DldRegistryEntriesHashTable::CreateStaticTableWithSize( 100, false ) failed\n" ) );
        
        goto __exit_on_error;
    }
    
    if( KERN_SUCCESS != DldSCSITask::DldInitSCSITaskSubsystem() ){
        
        assert( !"DldSCSITask::DldInitSCSITaskSubsystem() failed" );
        DBG_PRINT_ERROR(("DldSCSITask::DldInitSCSITaskSubsystem() failed\n"));
        
        goto __exit_on_error;
    }
    
    DBG_PRINT(("DldRegistryEntriesHashTable::CreateStaticTableWithSize( 100, false ) returned success\n"));
    
    //
    // init log
    //
    gLog = DldIOLog::newLog();
    assert( gLog );
    if( !gLog ) {
        
        DBG_PRINT_ERROR( ( "DldIOLog::newLog() failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gLog = (void*)gLog;
    DBG_PRINT(("DldIOLog::newLog() returned success gLog=0x%p\n", (void*)gLog ));
    
    //
    // init shadow
    //
    DldShadowObjectInitializer diskShadowInitializer;
    bzero( &diskShadowInitializer, sizeof(diskShadowInitializer) );
    diskShadowInitializer.queueSize = 0x1000*0x100;//1 MB
    
    gShadow = DldIOShadow::withInitializer( &diskShadowInitializer );
    assert( gShadow );
    if( !gShadow ){
        
        DBG_PRINT_ERROR( ( "DldIOShadow::withInitializer() failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gShadow = (void*)gShadow;
    DBG_PRINT(("DldIOShadow::withInitializer( &initializer ) returned success gShadow=0x%p\n", (void*)gShadow ));
    
#ifdef _DLD_MACOSX_CAWL
    //
    // init disk CAWL
    //
    DldDiskCAWLObjectInitializer diskCAWLInitializer;
    bzero( &diskCAWLInitializer, sizeof(diskCAWLInitializer) );
    
    gDiskCAWL = DldDiskCAWL::withInitializer( &diskCAWLInitializer );
    assert( gDiskCAWL );
    if( !gDiskCAWL ){
        
        DBG_PRINT_ERROR( ( "DldDiskCAWL::withInitializer() failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gDiskCAWL = (void*)gDiskCAWL;
    DBG_PRINT(("DldDiskCAWL::withInitializer( &initializer ) returned success gDiskCAWL=0x%p\n", (void*)gDiskCAWL ));    
#endif // _DLD_MACOSX_CAWL
    
    gWhiteList = DldWhiteList::newWhiteList();
    assert( gWhiteList );
    if( !gWhiteList ){
        
        DBG_PRINT_ERROR( ( "DldWhiteList::newWhiteList() failed\n" ) );
        goto __exit_on_error;
    }
    
    gCDDVDWhiteList = DldDVDWhiteList::withDefaultSettings();
    assert( gCDDVDWhiteList );
    if( !gCDDVDWhiteList ){
        
        DBG_PRINT_ERROR( ( "DldDVDWhiteList::withDefaultSettings() failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gCDDVDWhiteList = (void*)gCDDVDWhiteList;
    
    gDldDbgData.gWhiteList = (void*)gWhiteList;
    DBG_PRINT(("DldWhiteList::newWhiteList() returned success gWhiteList=0x%p\n", (void*)gWhiteList ));
    
    //
    // init the credentials array
    //
    gCredsArray = DldKauthCredArray::withCapacity( 0x4 );
    assert( gCredsArray );
    if( !gCredsArray ){
        
        DBG_PRINT_ERROR( ( "DldKauthCredArray::withCapacity( 0x4 ) failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gCredsArray = (void*)gCredsArray;
    DBG_PRINT(("DldKauthCredArray::withCapacity( 0x4 ) returned success gCredsArray=0x%p\n", (void*)gCredsArray ));
    
    //
    // create the registry root
    //
    if( kIOReturnSuccess != DldCreateRegistryRoot() ){
        
        DBG_PRINT_ERROR( ( "DldCreateRegistryRoot() failed\n" ) );
        goto __exit_on_error;
    }
    
    DBG_PRINT(("DldCreateRegistryRoot() returned success gDldRootEntry=0x%p\n", (void*)gDldRootEntry ));
    assert( gDldRootEntry );
    
    //
    // fill the class hooks dictionary and register hook callbacks for the classes, the callbacks will be called
    // just after registration for already existing objects!
    //
    gHookEngine = DldIOKitHookEngine::withNoHooks();
    assert( NULL != gHookEngine );
    if( NULL == gHookEngine ) {
        
        DBG_PRINT_ERROR( ( "DldIOKitHookEngine::withNoHooks() failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gHookEngine = (void*)gHookEngine;
    DBG_PRINT(("DldIOKitHookEngine::withNoHooks() returned success gHookEngine=0x%p\n", (void*)gHookEngine ));
    
    if( !gHookEngine->startHookingWithPredefinedClasses() ){
        
        DBG_PRINT_ERROR( ( "gHookEngine->startHookingWithPredefinedClasses() failed\n" ) );
        goto __exit_on_error;
    }
    
    DBG_PRINT(("gHookEngine->startHookingWithPredefinedClasses() returned success\n" ));
    
    //CopyIoServicePlane( gIOServicePlane, gHookEngine );
    
    gServiceProtection = DldServiceProtection::createServiceProtectionObject();
    assert( gServiceProtection );
    if( gServiceProtection ){
        
        gDldDbgData.gServiceProtection = (void*)gServiceProtection;
        gServiceProtection->startProtection();
        
    } else {
        
        DBG_PRINT_ERROR(("DldServiceProtection::createServiceProtectionObject failed\n"));
        goto __exit_on_error;
    }
    
    //
    // create an object for the vnodes KAuth callback and register the callback,
    // the callback might be called immediatelly just after registration!
    //
    gVnodeGate = DldIOKitKAuthVnodeGate::withCallbackRegistration();
    assert( NULL != gVnodeGate );
    if( NULL == gVnodeGate ){
        
        DBG_PRINT_ERROR( ( "DldIOKitKAuthVnodeGate::withDefaultSettings() failed\n" ) );
        goto __exit_on_error;
    }
    
    gDldDbgData.gVnodeGate = (void*)gVnodeGate;
    DBG_PRINT(("DldIOKitKAuthVnodeGate::withCallbackRegistration() returned success gVnodeGate=0x%p\n", (void*)gVnodeGate ));
    
#ifdef _DLD_SOCKET_FILTER
    //
    // start network filtering
    //
    if( KERN_SUCCESS == DldSocketFilter::InitSocketFilterSubsystem() ){
        
        gSocketFilter = DldSocketFilter::withDefault();
        assert( gSocketFilter );
        if( NULL == gSocketFilter ){
            
            DBG_PRINT_ERROR(("DldSocketFilter::withDefault() fauled\n"));
            goto __exit_on_error;
        }
        
        gDldDbgData.gSocketFilter = (void*)gSocketFilter;
        DBG_PRINT(("DldSocketFilter::withDefault() returned success gSocketFilter=0x%p\n", (void*)gSocketFilter ));
        
        gSocketFilter->startFilter();
        
    } else {
        
        DBG_PRINT_ERROR(("DldSocketFilter::InitSocketFilterSubsystem() fauled\n"));
        goto __exit_on_error;
        
    }
#endif // _DLD_SOCKET_FILTER
    
    /*
    DBG_PRINT( ( "com_devicelock_driver_DeviceLockIOKitDriver gIOServicePlane: \n" ) );
    _this->IOPrintPlane( gIOServicePlane );
    
    DBG_PRINT( ( "com_devicelock_driver_DeviceLockIOKitDriver gIODTPlane: \n" ) );
    _this->IOPrintPlane( gIODTPlane );
     */
    
#if defined(DBG) && defined(_DLD_MACOSX_CAWL)
    //DldSparseFile::test();
#endif // DBG && _DLD_MACOSX_CAWL
    
    //
    // wakeup a thread that called ::start()
    //
    _this->initializationWasSuccessful = true;
    DldSetNotificationEvent( &_this->initializationCompletionEvent );
    
    //
    // the object has been referenced when the object was provided as a thread parameter
    //
    _this->release();
    return;
    
__exit_on_error:
    
    _this->initializationWasSuccessful = false;
    DldSetNotificationEvent( &_this->initializationCompletionEvent );
    
    //
    // all cleanup is done in stop() and free()
    //
    
    //
    // the object has been referenced when the object was provided as a thread parameter
    //
    if( _this )
        _this->release();
    
    thread_terminate( current_thread() );
}

//--------------------------------------------------------------------

IOReturn
com_devicelock_driver_DeviceLockIOKitDriver::DumpTree( void )
{
    OSDictionary         * dict;
    OSIterator           * iter;
    IOService            * service;
    IOReturn               ret;
    
    ret = kIOReturnSuccess;
    
    iter = IORegistryIterator::iterateOver( gIODTPlane,
                                            kIORegistryIterateRecursively );
    assert( iter );
    
    if( !iter )
        return kIOReturnNoMemory;
    
    do{
        
        while( (service = (IOService *)iter->getNextObject()) ) {
            dict = service->getPropertyTable();
            if( !dict )
                continue;
            
            OSString    *ClassName;
            
            ClassName = (OSString*)dict->getObject( gIOClassKey );
            
            if( !ClassName )
                continue;
                
            DBG_PRINT( ( "com_devicelock_driver_DeviceLockIOKitDriver->DumpTree: found name %s \n", (char*)ClassName->getCStringNoCopy() ) );
            
            /*
            if( 0x0 == strcmp( (char*)ClassName->getCStringNoCopy() ,"IOUSBMassStorageClass" ) ) {
                
                IOUSBMassStorageClass*   USBMassStorageClassInstancePtr;
                
                __asm__ volatile( "int $0x3" );
                
                USBMassStorageClassInstancePtr = (IOUSBMassStorageClass*)service;
                
                IOUSBMassStorageClassDldHook::HookObject( USBMassStorageClassInstancePtr );

            }
             */
            
            //
            // allow the system to replenish the log buffer by removing the existing entries
            //
            IOSleep(350);
            
        }// end while
        
    } while( !service && !iter->isValid());
    
    iter->release();
    
    return ret;
    
}

//--------------------------------------------------------------------

IOReturn
com_devicelock_driver_DeviceLockIOKitDriver::IOPrintPlane(
    const IORegistryPlane * plane
    )
{
    IOReturn                ret;
    IORegistryEntry *		next;
    IORegistryIterator * 	iter;
    //char			        format[] = "%xxxs";
    IOService *			    service;
    
    ret = kIOReturnSuccess;
    
    iter = IORegistryIterator::iterateOver( plane );
    assert( iter );
    if( !iter )
        return kIOReturnNoMemory;
    
    iter->reset();
    while( ( next = iter->getNextObjectRecursive() ) ) {
        
       // snprintf( format + 1, sizeof(format) - 1, "%ds", 2 * next->getDepth( plane ) );
        
       // DBG_PRINT( (format, "") );
        DBG_PRINT( ( "%s", next->getName( plane ) ) );
        
        if( (next->getLocation( plane ) ) ){
            
            DBG_PRINT( (" %s", next->getLocation( plane ) ) );
            
        }// end if
        
        DBG_PRINT( ( " <class %s", next->getMetaClass()->getClassName() ) );
        
        
        if( ( service = OSDynamicCast(IOService, next) ) ){
            
            DBG_PRINT( ( ", busy %ld", (long) service->getBusyState() ) );
            
            assert( NULL != gHookEngine );
            
            gHookEngine->HookObject( (OSObject*)next );
            
            /*
            if( 0x0 == strcmp( next->getMetaClass()->getClassName() ,"IOUSBMassStorageClass" ) ) {
                
                IOUSBMassStorageClass*   USBMassStorageClassInstancePtr;
                
                __asm__ volatile( "int $0x3" );
                
                USBMassStorageClassInstancePtr = (IOUSBMassStorageClass*)service;
                
                IOUSBMassStorageClassDldHook::HookObject( USBMassStorageClassInstancePtr );
                
            }// end if( strcmp
             */
            
        }// end if( service
        
        DBG_PRINT( (">\n") );
        
        //
        // allow the system to replenish the log buffer by removing the existing entries
        //
        IOSleep(350);
    }
    iter->release();
    
    return ret;
}

//--------------------------------------------------------------------

bool
DldAddNewIORegistryEntryToDldPlane(
    __in IORegistryEntry *  object
    )
/*
 the main function for a new DldIOService creation and adding to the DldPlane tree,
 returns the true value if the entry has been added and the false value
 if there was an error or the entry has been already added, so "false"
 doesn't necessary mean an "error"
 */
{
    IOService*    service;
    
    assert( DldRegistryEntriesHashTable::sHashTable );
    assert( gDldDeviceTreePlan );
    
    if( !DldRegistryEntriesHashTable::sHashTable )
        return false;
    
    if( !( service = OSDynamicCast( IOService, object ) ) )
        return false;
    
    bool bFoundInHash = false;
    bool bAddedInHash = false;
    
    DldIOService* serviceEntry = NULL;
    
    DldRegistryEntriesHashTable::sHashTable->LockShared();
    {// start of the lock
        
        assert( false == bFoundInHash );
        
        DldIORegistryEntry*  registryEntry;
        
        //
        // try to retrieve an already added object
        //
        registryEntry = DldRegistryEntriesHashTable::sHashTable->RetrieveObjectEntry( service, true );
        if( registryEntry ){
            
            serviceEntry = OSDynamicCast( DldIOService, registryEntry ); // serviceEntry consumes the registryEntry reference
            assert( serviceEntry );
            if( ! serviceEntry ){
                
                registryEntry->release();
                DLD_DBG_MAKE_POINTER_INVALID( registryEntry );
            }
        } // end if( registryEntry )
        
    }// end of the lock
    DldRegistryEntriesHashTable::sHashTable->UnLockShared();
    
    bFoundInHash = ( NULL != serviceEntry );
    
    //
    // if has been found in the hash table then the corresponding Dld object has been already created and added
    //
    if( ! bFoundInHash ){
        
        //
        // allocate a new object before acquiring the lock as creating a new
        // entry under the lock protection is an overkill solution prone to deadlocks,
        // do not update property here as the parent properties for the object are
        // not known, they will be set at attach processing( look further )
        //
        serviceEntry = DldIOService::withIOService( service, true );
        assert( serviceEntry );
        if( !serviceEntry )
            return false;
        
        DldRegistryEntriesHashTable::sHashTable->LockExclusive();
        {// start of the lock
            
            assert( false == bFoundInHash );
            
            //
            // check that the DldIoService object for the service object
            // has not been already inserted in the hash table
            //
            bFoundInHash = DldRegistryEntriesHashTable::sHashTable->RetrieveObjectEntry( service, false );
            if( !bFoundInHash ){
                
                //
                // add a new object in the hash table
                //
                bAddedInHash = DldRegistryEntriesHashTable::sHashTable->AddObject( service, serviceEntry ) ;
                assert( bAddedInHash );
                
            }
            
        }// end of the lock
        DldRegistryEntriesHashTable::sHashTable->UnLockExclusive();
        
        if( !bAddedInHash ){
            
            assert( DLD_IS_POINTER_VALID( serviceEntry ) );
            
            if( !bFoundInHash ){
                
                assert( !"DldProcessNewIORegistryEntry()->DldRegistryEntriesHashTable::sHashTable->AddObject( service, Entry ) failed" );
                DBG_PRINT_ERROR( ( "DldProcessNewIORegistryEntry()->DldRegistryEntriesHashTable::sHashTable->AddObject( service, Entry ) failed\n" ) );
                
            }// end if( !FoundInHash )
            
            //
            // the entry was not added to the hash tabe
            //
            serviceEntry->release();
            DLD_DBG_MAKE_POINTER_INVALID( serviceEntry );
            
            return bFoundInHash;
            
        }// end if( !bAdded )
        
    } // end if( ! bFoundInHash )
        
    //
    // attach to all parents, there can be multiple parents case, including IOResources,
    // e.g. a single IOHIDSystem class instance is attached to all HID device stacks
    // and to the IOResource object
    //
    OSIterator* parentIterator = service->getParentIterator( gIOServicePlane );
    if( parentIterator ){
        
        OSObject*   object = NULL;
        
        while( NULL != (object = parentIterator->getNextObject()) ){
            
            IOService*  parentService = NULL;
            
            //
            // check if we hit the root
            //
            if( NULL == ( parentService = OSDynamicCast( IOService, object ) ) ){

                //
                // there is no parent's IOService or IORegistryEntry entry, so attach to the root
                //
                serviceEntry->attachToParent( gDldRootEntry, gDldDeviceTreePlan );
                serviceEntry->start( service, NULL );
                
                continue;
            } // end if( NULL == ( parentService = OSDynamicCast( IOService, object ) ) )
            
            DldIOService* parentEntry;
            
            parentEntry = DldIOService::RetrieveDldIOServiceForIOService( parentService );
            if( parentEntry ){
                
                serviceEntry->attach( parentEntry );
                serviceEntry->start( service, parentEntry );
                
                //
                // was referenced by RetrieveDldIOServiceForIOService
                //
                parentEntry->release();
                DLD_DBG_MAKE_POINTER_INVALID( parentEntry );
                
            } else {
                
                //
                // there is no parent's DldIOService entry, so attach to the root
                //
                serviceEntry->attachToParent( gDldRootEntry, gDldDeviceTreePlan );
                serviceEntry->start( service, NULL );
            }
            
        } // end while
        
        parentIterator->release();
        DLD_DBG_MAKE_POINTER_INVALID( parentIterator );
        
    }  else {
        
        //
        // there is no parent's IOService or IORegistryEntry entry, so attach to the root
        //
        serviceEntry->attachToParent( gDldRootEntry, gDldDeviceTreePlan );
        serviceEntry->start( service, NULL );
    }
    
    IORegistryEntry*  parentRegEntry;
    IOService*        parentService = NULL;
    
    parentRegEntry = service->getParentEntry( gIOServicePlane );
    if( parentRegEntry )
        parentService = OSDynamicCast( IOService, parentRegEntry );
    
    if( parentService ){
        
        DldIOService* parentEntry;
        
        parentEntry = DldIOService::RetrieveDldIOServiceForIOService( parentService );
        if( parentEntry ){
            
            serviceEntry->attach( parentEntry );
            serviceEntry->start( service, parentEntry );
            
            //
            // was referenced by RetrieveDldIOServiceForIOService
            //
            parentEntry->release();
            DLD_DBG_MAKE_POINTER_INVALID( parentEntry );
            
        } else {
            
            //
            // there is no parent's DldIOService entry, so attach to the root
            //
            serviceEntry->attachToParent( gDldRootEntry, gDldDeviceTreePlan );
            serviceEntry->start( service, NULL );
        }
        
        
    } else {
        
        //
        // there is no parent's IOService or IORegistryEntry entry, so attach to the root
        //
        serviceEntry->attachToParent( gDldRootEntry, gDldDeviceTreePlan );
        serviceEntry->start( service, NULL );
    }
    
    //
    // assume that the user client has been attached as the device must have been initialized
    //
    serviceEntry->userClientAttached();
    
    serviceEntry->release();
    DLD_DBG_MAKE_POINTER_INVALID( serviceEntry );
    
    return true;
}

//--------------------------------------------------------------------

IOReturn
DldCreateRegistryRoot()
{
    IOReturn  ret = kIOReturnSuccess;
    
    //
    // int the tree root
    //
    gDldRootEntry = new DldIORegistryEntry();
    assert( gDldRootEntry );
    if( !gDldRootEntry ){
        
        DBG_PRINT_ERROR( ( "gDldRootEntry = new DldIORegistryEntry() failed\n" ) );
        return kIOReturnError;
    }
    
    gDldDbgData.gDldRootEntry = (void*)gDldRootEntry;
    
    IORegistryEntry * registryRoot = IORegistryEntry::getRegistryRoot();
    assert( registryRoot );
    if( !registryRoot ){
        
        gDldRootEntry->release();
        gDldRootEntry = NULL;
        
        return kIOReturnError;
    }
    
    if( !gDldRootEntry->init() ){
        
        assert( !"CopyIoServicePlane()->DldRootEntry->init()  failed" );
        DBG_PRINT_ERROR( ( "DldRootEntry->init()  failed\n" ) );
        
        gDldRootEntry->release();
        gDldRootEntry = NULL;
        
        return kIOReturnError;
    }
    
    gDldRootEntry->attachToParent( registryRoot, gDldDeviceTreePlan );
    
    return ret;
}

//--------------------------------------------------------------------

IOReturn
CopyIoServicePlane(
    __in const IORegistryPlane* plane,
    __in DldIOKitHookEngine*    HookEngine
    )
{
    IOReturn                ret = kIOReturnSuccess;
    IORegistryEntry *		next;
    IORegistryIterator * 	iter;
#if defined( DBG )
    bool                    lockState = false;
#endif//DBG
    
    assert( gDldRootEntry );
        
    iter = IORegistryIterator::iterateOver( plane );
    assert( iter );
    if( !iter )
        return kIOReturnNoMemory;
    
    iter->reset();
    while( ( next = iter->getNextObjectRecursive() ) ){
        
#if defined( DBG )
        assert( !lockState );
#endif//#if defined( DBG )
        
        IOService*   service = NULL;
        bool         locked  = false;
        
        service = OSDynamicCast( IOService, next );
        if( service ){
            
            //
            // do not lock if the object is being terminated
            //
            locked = service->lockForArbitration( false );
#if defined( DBG )
            lockState = locked;
#endif//#if defined( DBG )
            if( !locked ){
                
                //
                // skip the terminated object
                //
                DBG_PRINT_ERROR(("lockForArbitration() failed for 0x%p( %s )\n", service, service->getMetaClass()->getClassName()));
                continue;
            }
            
        }// end if( service )
        
        
        if( DldAddNewIORegistryEntryToDldPlane( next ) ){
            
            //
            // hook the object for which the shadow object has been created
            //
            HookEngine->HookObject( next );
        }
        
        
        if( locked ){
            
            assert( service );
            service->unlockForArbitration();
            locked = false;
            
        }// end if( locked )
        
#if defined( DBG )
        lockState = locked;
#endif//#if defined( DBG )
        
    }// end while( ( next = iter->getNextObjectRecursive() ) )
    iter->release();
    
#if defined( DBG )
    assert( !lockState );
#endif//#if defined( DBG )
    
    return ret;
    
}

//--------------------------------------------------------------------

IOReturn
com_devicelock_driver_DeviceLockIOKitDriver::newUserClient(
    __in task_t owningTask,
    __in void * securityID,
    __in UInt32 type,
    __in IOUserClient **handler
    )
{
    
    DldIOUserClient*  client = NULL;
    
    //
    // Check that this is a user client type that we support.
    // type is known only to this driver's user and kernel
    // classes. It could be used, for example, to define
    // read or write privileges. In this case, we look for
    // a private value.
    //
    if( type != kDldUserClientCookie )
        return kIOReturnBadArgument;

    //
    // Construct a new client instance for the requesting task.
    // This is, essentially  client = new DldIOUserClient;
    //                               ... create metaclasses ...
    //                               client->setTask(owningTask)
    //
    client = DldIOUserClient::withTask( owningTask );
    assert( client );
    if( client == NULL ){
        
        DBG_PRINT_ERROR(("Can not create a user client for the task = %p\n", (void*)owningTask ));
        return kIOReturnNoResources;
    }

    //
    // Attach the client to our driver
    //
    if( !client->attach( this ) ) {
        
        assert( !"client->attach( this ) failed" );
        DBG_PRINT_ERROR(("Can attach a user client for the task = %p\n", (void*)owningTask ));
        
        client->release();
        return kIOReturnNoResources;
    }

    //
    // Start the client so it can accept requests
    //
    if( !client->start( this ) ){
        
        assert( !"client->start( this ) failed" );
        DBG_PRINT_ERROR(("Can start a user client for the task = %p\n", (void*)owningTask ));
        
        client->detach( this );
        client->release();
        return kIOReturnNoResources;
    }
    
    gDldDbgData.userClient = client;

    *handler = client;
    return kIOReturnSuccess;
}

//--------------------------------------------------------------------

#undef super




