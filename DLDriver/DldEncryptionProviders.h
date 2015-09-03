/*
 *  DldEncryptionProviders.h
 *  DeviceLock
 *
 *  Created by Slava on 1/04/13.
 *  Copyright 2013 Slava Imameev. All rights reserved.
 *
 */
#ifndef _DLDENCRYPTIONPROVIDER_H
#define _DLDENCRYPTIONPROVIDER_H

#include "DldUserToKernel.h"

typedef struct _DldEncrytionProvider{
    bool   enabled;
} DldEncrytionProvider;

extern DldEncrytionProvider    gEncryptionProvider[ MaximumEncryptionProviderNumber ];

#endif // _DLDENCRYPTIONPROVIDER_H
