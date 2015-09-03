/* 
 * Copyright (c) 2011 Slava Imameev. All rights reserved.
 */

#ifndef _DLDNETWORKINTERFACEFILTER_H
#define _DLDNETWORKINTERFACEFILTER_H

#include <net/if_var.h>
#include <net/kpi_interfacefilter.h>
#include "DldCommon.h"
#include "DldIOService.h"
#include "DldHookerCommonClass.h"

class DldNetworkInterfaceFilter: public OSObject
{
    OSDeclareDefaultStructors( DldNetworkInterfaceFilter )
    
private:
    
    //
    // an IOService for the related interface, the object is not referenced
    // to not create a reference cycle, must be inherited from IOEthernetInterface
    //
    IOService*              interfaceObject;
    
    //
    // filter structure
    //
    struct iff_filter       iffFilter;
    
    //
    // a filter descriptor returned by the system from iflt_attach()
    //
    interface_filter_t      interfaceFilter;
    
    //
    // a corresponding BSD net interface, a referenced pointer
    //
    ifnet_t                 bsdInterface;
    
    //
    // a supporting class, use carefully as there is no connection with hooked object
    //
    DldHookerCommonClass    commonHooker;
    
    /*!
     @typedef iff_input_func
     
     @discussion iff_input_func is used to filter incoming packets. The
     interface is only valid for the duration of the filter call. If
     you need to keep a reference to the interface, be sure to call
     ifnet_reference and ifnet_release. The packets passed to the
     inbound filter are different from those passed to the outbound
     filter. Packets to the inbound filter have the frame header
     passed in separately from the rest of the packet. The outbound
     data filters is passed the whole packet including the frame
     header.
     
     The frame header usually preceeds the data in the mbuf. This
     ensures that the frame header will be a valid pointer as long as
     the mbuf is not freed. If you need to change the frame header to
     point somewhere else, the recommended method is to prepend a new
     frame header to the mbuf chain (mbuf_prepend), set the header to
     point to that data, then call mbuf_adj to move the mbuf data
     pointer back to the start of the packet payload.
     @param cookie The cookie specified when this filter was attached.
     @param interface The interface the packet was recieved on.
     @param protocol The protocol of this packet. If you specified a
     protocol when attaching your filter, the protocol will only ever
     be the protocol you specified.
     @param data The inbound packet, after the frame header as determined
     by the interface.
     @param frame_ptr A pointer to the pointer to the frame header. The
     frame header length can be found by inspecting the interface's
     frame header length (ifnet_hdrlen).
     @result Return:
     0 - The caller will continue with normal processing of the
     packet.
     EJUSTRETURN - The caller will stop processing the packet,
     the packet will not be freed.
     Anything Else - The caller will free the packet and stop
     processing.
     */
    static errno_t dld_iff_input_func(void *cookie, ifnet_t interface,
                                  protocol_family_t protocol, mbuf_t *data, char **frame_ptr);
    
    /*!
     @typedef iff_output_func
     
     @discussion iff_output_func is used to filter fully formed outbound
     packets. The interface is only valid for the duration of the
     filter call. If you need to keep a reference to the interface,
     be sure to call ifnet_reference and ifnet_release.
     @param cookie The cookie specified when this filter was attached.
     @param interface The interface the packet is being transmitted on.
     @param data The fully formed outbound packet in a chain of mbufs.
     The frame header is already included. The filter function may
     modify the packet or return a different mbuf chain.
     @result Return:
     0 - The caller will continue with normal processing of the
     packet.
     EJUSTRETURN - The caller will stop processing the packet,
     the packet will not be freed.
     Anything Else - The caller will free the packet and stop
     processing.
     */
    static errno_t dld_iff_output_func(void *cookie, ifnet_t interface,
                                       protocol_family_t protocol, mbuf_t *data);
    
    /*!
     @typedef iff_event_func
     
     @discussion iff_event_func is used to filter interface specific
     events. The interface is only valid for the duration of the
     filter call. If you need to keep a reference to the interface,
     be sure to call ifnet_reference and ifnet_release.
     @param cookie The cookie specified when this filter was attached.
     @param interface The interface the packet is being transmitted on.
     @param event_msg The kernel event, may not be changed.
     */
    //static void dld_iff_event_func(void *cookie, ifnet_t interface,
    //                               protocol_family_t protocol, const struct kev_msg *event_msg);
    
    /*!
     @typedef iff_ioctl_func
     
     @discussion iff_ioctl_func is used to filter ioctls sent to an
     interface. The interface is only valid for the duration of the
     filter call. If you need to keep a reference to the interface,
     be sure to call ifnet_reference and ifnet_release.
     
     All undefined ioctls are reserved for future use by Apple. If
     you need to communicate with your kext using an ioctl, please
     use SIOCSIFKPI and SIOCGIFKPI.
     @param cookie The cookie specified when this filter was attached.
     @param interface The interface the packet is being transmitted on.
     @param ioctl_cmd The ioctl command.
     @param ioctl_arg A pointer to the ioctl argument.
     @result Return:
     0 - This filter function handled the ioctl.
     EOPNOTSUPP - This filter function does not understand/did not
     handle this ioctl.
     EJUSTRETURN - This filter function handled the ioctl,
     processing should stop.
     Anything Else - Processing will stop, the error will be
     returned.
     */
    //static errno_t dld_iff_ioctl_func(void *cookie, ifnet_t interface,
    //                                  protocol_family_t protocol, unsigned long ioctl_cmd, void *ioctl_arg);
    
    /*!
     @typedef iff_detached_func
     
     @discussion iff_detached_func is called to notify the filter that it
     has been detached from an interface. This is the last call to
     the filter that will be made. A filter may be detached if the
     interface is detached or the detach filter function is called.
     In the case that the interface is being detached, your filter's
     event function will be called with the interface detaching event
     before the your detached function will be called.
     @param cookie The cookie specified when this filter was attached.
     @param interface The interface this filter was detached from.
     */
    static void dld_iff_detached_func(void *cookie, ifnet_t interface);
    
protected:
    
    virtual void free();
    virtual bool init();
    
public:
    
    //
    // networkInterface must describe the object derived from IOEthernetInterface
    //
    static DldNetworkInterfaceFilter* withNetworkInterface(__in DldIOService*   networkInterface);
    
    errno_t attachToNetworkInterface();
    
};

#endif//_DLDNETWORKINTERFACEFILTER_H

