/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */
#include "DldUserCommon.h"
#include "DldDriverConnection.h"
#include "DldLogHandler.h"
#include "DldAcl.h"
#include "DldShadow.h"


int main (int argc, char * const argv[]) {

    io_connect_t    connection;
    kern_return_t   kr;
    int             ret;
    pthread_t       dataQueueThread;
    pthread_t       shadowQueueThread;
    
    
    kr = DldOpenDlDriver( &connection );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    //
    // set a shadow file
    //
    const char* shadow_file_name1 = "/work/shadow1.txt";
    const char* shadow_file_name2 = "/work/shadow2.txt";
    const char* shadow_file_name3 = "/work/shadow3.txt";
    const char* shadow_file_name4 = "/work/shadow4.txt";
    const char* shadow_file_name5 = "/work/shadow5.txt";
    
    kr = DldSetShadowFile( connection, (char*)shadow_file_name1, 0x10ll*0x1000ll*0x1000ll , 0x1 );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    kr = DldSetShadowFile( connection, (char*)shadow_file_name2, 0x10ll*0x1000ll*0x1000ll , 0x2 );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    kr = DldSetShadowFile( connection, (char*)shadow_file_name3, 0x10ll*0x1000ll*0x1000ll , 0x3 );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    kr = DldSetShadowFile( connection, (char*)shadow_file_name4, 0x10ll*0x1000ll*0x1000ll , 0x4 );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    kr = DldSetShadowFile( connection, (char*)shadow_file_name5, 0x10ll*0x1000ll*0x1000ll , 0x5 );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    //
    // removable + usb
    //
    DldDeviceType  deviceType;
    const char* acl_str_rem = "!#acl 1\n"
                              "user::Slava::deny:read,write\n"
                              //"group::Administrators::deny:write\n"
                              "user::test1::deny:write";
        
    deviceType.type.major = DLD_DEVICE_TYPE_REMOVABLE;
    deviceType.type.minor = DLD_DEVICE_TYPE_USB;
    
    kr = DldSetAclFromText( connection,
                            deviceType,
                            kDldAclTypeSecurity,
                            (char*)acl_str_rem );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    //
    // usb
    //
    const char* acl_str_usb = "!#acl 1\n"
                              "user::Slava::deny:read,write\n"
                               "user::root::deny:read,write\n"
                              //"group::Administrators::deny:write\n"
                              "user::test1::deny:write";
    
    deviceType.type.major = DLD_DEVICE_TYPE_USB;
    deviceType.type.minor = DLD_DEVICE_TYPE_UNKNOWN;
    
    kr = DldSetAclFromText( connection,
                            deviceType,
                            kDldAclTypeSecurity,
                            (char*)acl_str_usb );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    //
    // USB logging
    //
    const char* acl_str_log = "!#acl 1\n"
                              "user::Slava::allow:read,write\n"
                              "user::root::allow:read,write\n"
                              //"group::Administrators::allow:write\n"
                              "user::test1::allow:read,write";
    
    deviceType.type.major = DLD_DEVICE_TYPE_REMOVABLE;
    deviceType.type.minor = DLD_DEVICE_TYPE_UNKNOWN;
    
    kr = DldSetAclFromText( connection,
                           deviceType,
                           kDldAclTypeLogAllowed,
                           (char*)acl_str_log );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    kr = DldSetAclFromText( connection,
                            deviceType,
                            kDldAclTypeLogDenied,
                            (char*)acl_str_log );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    
    //
    // usb shadowing
    //
    const char* acl_str_usb_shadow = "!#acl 1\n"
                              "user::Slava::allow:write\n"
                              //"user::root::allow:read,write\n"
                              //"group::Administrators::allow:write\n"
                              "user::test1::allow:write";
    
    deviceType.type.major = DLD_DEVICE_TYPE_USB;
    deviceType.type.minor = DLD_DEVICE_TYPE_UNKNOWN;
    
    kr = DldSetAclFromText( connection,
                            deviceType,
                            kDldAclTypeShadow,
                            (char*)acl_str_usb_shadow );
    if( KERN_SUCCESS != kr ){
        
        return (-1);
    }
    
    ret = pthread_create(&dataQueueThread, (pthread_attr_t *)0,
                         (void* (*)(void*))vnodeNotificationHandler, (void *)connection);
    if (ret){
        perror("pthread_create( vnodeNotificationHandler )");
        dataQueueThread = NULL;
    }
    
    ret = pthread_create(&shadowQueueThread, (pthread_attr_t *)0,
                         (void* (*)(void*))shadowNotificationHandler, (void *)connection);
    if (ret){
        perror("pthread_create( shadowNotificationHandler )");
        shadowQueueThread = NULL;
    }
    
    if( dataQueueThread )
        pthread_join(dataQueueThread, (void **)&kr);
    
    if( shadowQueueThread )
        pthread_join(shadowQueueThread, (void **)&kr);
    
    IOServiceClose(connection);

    return 0;
    
}
