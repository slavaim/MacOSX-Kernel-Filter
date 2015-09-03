/*
 *  DldServiceProtection.h
 *  DeviceLock
 *
 *  Created by Slava on 4/06/12.
 *  Copyright 2012 Slava Imameev. All rights reserved.
 *
 */

#ifndef _DLDSERVICEPROTECTION_H
#define _DLDSERVICEPROTECTION_H

#include "DldMacPolicy.h"
#include "DldProtectedProcess.h"
#include "DldProtectedFile.h"
#include "DldIOUserClientRef.h"

class DldServiceProtection: public DldMacPolicy{
    
    OSDeclareDefaultStructors( DldServiceProtection )
    
    friend class DldProtectedFile;
    friend class com_devicelock_driver_DeviceLockIPCDriver;
    
private:
    
    static bool   mpcInit;
    static struct mac_policy_ops   mpoServiceProtection;
    static struct mac_policy_conf  mpcServiceProtection;
    
protected:
    virtual void free();
    virtual bool init();
    
private:
    //
    // MAC policy callbacks
    //
    /**
     @brief Access control check for delivering signal
     @param cred Subject credential
     @param proc Object process
     @param signum Signal number; see kill(2)
     
     Determine whether the subject identified by the credential can deliver
     the passed signal to the passed process.
     
     @warning Programs typically expect to be able to send and receive
     signals as part or their normal process lifecycle; caution should be
     exercised when implementing access controls over signal events.
     
     @return Return 0 if access is granted, otherwise an appropriate value for
     errno should be returned. Suggested failure: EACCES for label mismatch,
     EPERM for lack of privilege, or ESRCH to limit visibility.
     */
    static int procCheckSignal( kauth_cred_t cred,
                                struct proc *proc,
                                int signum );
    
    /**
     @brief Access control check for getting a process's task port
     @param cred Subject credential
     @param proc Object process
     
     Determine whether the subject identified by the credential can get
     the passed process's task control port.
     This call is used by the task_for_pid(2) API.
     
     @return Return 0 if access is granted, otherwise an appropriate value for
     errno should be returned. Suggested failure: EACCES for label mismatch,
     EPERM for lack of privilege, or ESRCH to hide visibility of the target.
     */
    static int procCheckGetTask( kauth_cred_t cred,
                                 struct proc *p );
    
    
    /**
     @brief Associate a credential with a new process at fork
     @param cred credential to inherited by new process
     @param proc the new process
     
     Allow a process to associate the credential with a new
     process for reference countng purposes.
     NOTE: the credential can be dis-associated in ways other
     than exit - so this strategy is flawed - should just
     catch label destroy callback.
     */
    static void credLabelAssociateFork( kauth_cred_t cred,
                                        proc_t proc );
    
private:
    
    //
    // KAUTH_SCOPE_PROCESS scope callback
    //
    static int KauthSopeProcessCallback( kauth_cred_t _credential,
                                         void *_idata,
                                         kauth_action_t _action,
                                         uintptr_t _arg0,
                                         uintptr_t _arg1,
                                         uintptr_t _arg2,
                                         uintptr_t _arg3);
    
private:
    
    //
    // called for each process started before the driver,
    // the function has an idempotent behaviour as can be called multiple time for a process
    //
    static int processIterateCallout( __in proc_t proc, __in void* context );
    
private:
    
    //
    // an array of DldProtectedProcess objects
    //
    OSArray*       protectedProcesses;
    
    //
    // a lock for protectedProcesses array
    //
    IORWLock*      protectedProcessesLock;
    
    //
    // a dictionary of protected files
    //
    OSDictionary*  protectedFiles;
    
    //
    // a lock for protectedFiles dictionary
    //
    IORWLock*      protectedFilesLock;
    
    //
    // a counter used to track the settings updates
    //
    SInt32         settingsSequenceCounter;
    
    //
    // a kaith listener for processes scope to catch ptrace system calls
    //
    kauth_listener_t  kauthProcessListener;
    
    //
    // a user client for the kernel-to-user communication
    //
    DldIOUserClientRef userClient;
    
    //
    // DL admins settings
    //
    DldDLAdminsSettings   adminSettings;
    
private:
    
    //
    // returns a non referenced object, a caller must hold the lock thus protecting object from removal
    //
    DldProtectedProcess* getProtectedProcessDscrByPidWithoutLock( __in pid_t pid, __out unsigned int* index );
    
protected:
    
    bool isAccessAllowed( __in_opt DldAclObject* aclObject,
                          __in kauth_ace_rights_t  requestedAccess,
                          __in kauth_cred_t cred );
    
private:
    //
    // a caller must call releaseUserClient() for each successfull call to getUserClient()
    //
    DldIOUserClient* getUserClient();
    void releaseUserClient();
    
public:
    
    static DldServiceProtection* createServiceProtectionObject();
    
    IOReturn startProtection();
    IOReturn stopProtection();
    
    IOReturn setProcessSecurity( __in pid_t pid, __in_opt DldAclObject* acl );
    void removeProcessSecurity( __in pid_t pid );
    void removeAllProcessesAndFilesSecurity();
    
    IOReturn setFileSecurity( __in OSString* path,
                                      __in_opt DldAclObject* usersAcl,
                                      __in_opt DldAclWithProcsObject* procsAcl );
    
    //
    // returns a referenced object or NULL, a caller must not hold a lock
    //
    DldProtectedProcess* getProtectedProcessRef( __in pid_t pid );
    
    //
    // returns a referenced object or NULL, a caller must not hold a lock
    //
    DldProtectedFile* getProtectedFileRef( __in const char* path );
    
    SInt32 getSettingsSequenceCounter() { return this->settingsSequenceCounter; }
    
    
    bool isUserClientPresent();
    IOReturn registerUserClient( __in DldIOUserClient* client );
    IOReturn unregisterUserClient( __in DldIOUserClient* client );
    
    void setDLAdminsSettings( __in const DldDLAdminsSettings* settings ) { this->adminSettings = *settings; }
    bool useDefaultSystemSecurity() { return ( 0x1 == this->adminSettings.EnableDefaultSecurity ); }
    bool protectAgentFiles() { return ( 0x1 == this->adminSettings.EnableSysFilesProtection || 0x0 == this->adminSettings.EnableDefaultSecurity ); }
};

extern DldServiceProtection* gServiceProtection;

#endif // _DLDSERVICEPROTECTION_H