/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#ifndef DLDPOWERMANAGEMENT_H
#define DLDPOWERMANAGEMENT_H

extern "C"{
    
    kern_return_t
    DldPowerManagementStart( void* object );
    
    kern_return_t
    DldPowerManagementStop();
}

#endif//DLDPOWERMANAGEMENT_H