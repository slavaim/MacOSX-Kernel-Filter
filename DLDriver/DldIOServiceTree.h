/*
 * Copyright (c) 2009 Slava Imameev. All rights reserved.
 */

#ifndef _DLDIOSERVICETREE_H
#define _DLDIOSERVICETREE_H

#include "DldCommon.h"
#include <sys/cdefs.h>
#include <sys/malloc.h>

class DldIOServiceTreeEntry: public OSObject{
    
    OSDeclareDefaultStructors( DldIOServiceTreeEntry )
    
public:
    
    static DldIOServiceTreeEntry* withIOService( __in IOService* serviceObject );
    //virtual bool CollectParentsInformation();
    
protected:
    
    virtual bool init();
    virtual void free();
    
private:
    IOService* serviceObject;
    OSArray* Parents;
    OSArray* Chidren;
    
};

#endif // _DLDIOSERVICETREE_H