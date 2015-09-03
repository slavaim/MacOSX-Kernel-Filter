/* 
 * Copyright (c) 2010 Slava Imameev. All rights reserved.
 */

#include "DldSCSITask.h"
#include "DldUserToKernel.h"
#include <sys/proc.h>

/*
 class SCSITask : public IOCommand
 {
 
 OSDeclareDefaultStructors ( SCSITask )
 
 private:
 
 // Object that owns the instantiation of the SCSI Task object.
 OSObject *					fOwner;
 
 // The member variables that represent each part of the Task
 // Task Management members
 SCSITaskAttribute			fTaskAttribute;
 SCSITaggedTaskIdentifier	fTaskTagIdentifier;
 SCSITaskState				fTaskState;
 SCSITaskStatus				fTaskStatus;
 
 // The intended Logical Unit Number for this Task.  Currently only single
 // level LUN values are supported.
 UInt8						fLogicalUnitNumber;
 
 SCSICommandDescriptorBlock	fCommandDescriptorBlock;
 UInt8						fCommandSize;
 UInt8						fTransferDirection;
 IOMemoryDescriptor *		fDataBuffer;
 UInt64						fDataBufferOffset;
 UInt64						fRequestedByteCountOfTransfer;
 UInt64						fRealizedByteCountOfTransfer;
 
 // Specifies the amount of time in milliseconds to wait for a task to
 // complete.  A zero value indicates that the task should be given the
 // longest duration possible by the Protocol Services Driver to complete.
 UInt32						fTimeoutDuration;
 
 SCSIServiceResponse			fServiceResponse;    
 
 SCSITaskCompletion			fCompletionCallback;
 
 // Autosense related members
 // This member indicates whether the client wants the SCSI Protocol
 // Layer to request autosense data if the command completes with a
 // CHECK_CONDITION status.
 bool						fAutosenseDataRequested;
 
 SCSICommandDescriptorBlock	fAutosenseCDB;
 UInt8						fAutosenseCDBSize;
 
 bool						fAutoSenseDataIsValid;
 SCSI_Sense_Data *			fAutoSenseData;
 UInt8						fAutoSenseDataSize;
 UInt64						fAutoSenseRealizedByteCountOfTransfer;
 
 IOMemoryDescriptor *		fAutosenseDescriptor;
 task_t						fAutosenseTaskMap;
 
 // Reference members for each layer.  Since these may contain a memory address, they
 // are declared as void * so that they will scale to a 64-bit system.
 void *						fProtocolLayerReference;
 void *						fApplicationLayerReference;
 
 // Pointer to the next SCSI Task in the queue.  This can only be used by the SCSI
 // Protocol Layer
 SCSITask *					fNextTaskInQueue;
 
 // The Task Execution mode is only used by the SCSI Protocol Layer for 
 // indicating whether the command currently being executed is the client's
 // command or the AutoSense RequestSense command.
 SCSITaskMode				fTaskExecutionMode;
 
 // Reserve space for future expansion.
 struct SCSITaskExpansionData { };
 SCSITaskExpansionData *		fSCSITaskReserved;
 
 public:
 
 virtual bool		init ( void );
 virtual void		free ( void );
 
 // Utility methods for setting and retreiving the Object that owns the
 // instantiation of the SCSI Task
 bool				SetTaskOwner ( OSObject	* taskOwner );
 OSObject *			GetTaskOwner ( void );
 
 // Utility method to reset the object so that it may be used for a new
 // Task.  This method will return true if the reset was successful
 // and false if it failed because it represents an active task.
 bool				ResetForNewTask ( void );
 
 // Utility method to check if this task represents an active.
 bool				IsTaskActive ( void );
 
 // Utility Methods for managing the Logical Unit Number for which this Task 
 // is intended.
 bool				SetLogicalUnitNumber ( UInt8 newLUN );
 UInt8				GetLogicalUnitNumber ( void );
 
 // The following methods are used to set and to get the value of the
 // task's attributes.  The set methods all return a bool which indicates
 // whether the attribute was successfully set.  The set methods will return
 // true if the attribute was updated to the new value and false if it was
 // not.  A false return value usually indicates that the task is active
 // and the attribute that was requested to be changed, can not be changed
 // once a task is active.
 // The get methods will always return the current value of the attribute
 // regardless of whether it has been previously set and regardless of
 // whether or not the task is active.	
 bool				SetTaskAttribute ( 
 SCSITaskAttribute newAttributeValue );
 SCSITaskAttribute	GetTaskAttribute ( void );
 
 bool				SetTaggedTaskIdentifier ( 
 SCSITaggedTaskIdentifier newTag );
 SCSITaggedTaskIdentifier GetTaggedTaskIdentifier ( void );
 
 bool				SetTaskState ( SCSITaskState newTaskState );
 SCSITaskState		GetTaskState ( void );
 
 // Accessor methods for getting and setting that status of the Task.	
 bool				SetTaskStatus ( SCSITaskStatus newTaskStatus );
 SCSITaskStatus		GetTaskStatus ( void );
 
 // Accessor functions for setting the properties of the Task
 
 // Methods for populating the Command Descriptor Block.  Individual methods
 // are used instead of having a single method so that the CDB size is
 // automatically known.
 // Populate the 6 Byte Command Descriptor Block
 bool 	SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5 );
 
 // Populate the 10 Byte Command Descriptor Block
 bool 	SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5,
 UInt8			cdbByte6,
 UInt8			cdbByte7,
 UInt8			cdbByte8,
 UInt8			cdbByte9 );
 
 // Populate the 12 Byte Command Descriptor Block
 bool 	SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5,
 UInt8			cdbByte6,
 UInt8			cdbByte7,
 UInt8			cdbByte8,
 UInt8			cdbByte9,
 UInt8			cdbByte10,
 UInt8			cdbByte11 );
 
 // Populate the 16 Byte Command Descriptor Block
 bool 	SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5,
 UInt8			cdbByte6,
 UInt8			cdbByte7,
 UInt8			cdbByte8,
 UInt8			cdbByte9,
 UInt8			cdbByte10,
 UInt8			cdbByte11,
 UInt8			cdbByte12,
 UInt8			cdbByte13,
 UInt8			cdbByte14,
 UInt8			cdbByte15 );
 
 UInt8	GetCommandDescriptorBlockSize ( void );
 
 // This will always return a 16 Byte CDB.  If the Protocol Layer driver
 // does not support 16 Byte CDBs, it will have to create a local 
 // SCSICommandDescriptorBlock variable to get the CDB data and then
 // transfer the needed bytes from there.
 bool	GetCommandDescriptorBlock ( 
 SCSICommandDescriptorBlock * cdbData );
 
 // Set up the control information for the transfer, including
 // the transfer direction and the number of bytes to transfer.
 bool	SetDataTransferDirection ( UInt8 newDataTransferDirection );
 UInt8	GetDataTransferDirection ( void );
 
 bool	SetRequestedDataTransferCount ( UInt64 requestedTransferCountInBytes );
 UInt64	GetRequestedDataTransferCount ( void );
 
 bool	SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes );
 UInt64	GetRealizedDataTransferCount ( void );
 
 bool	SetDataBuffer ( IOMemoryDescriptor * newDataBuffer );
 IOMemoryDescriptor * GetDataBuffer ( void );
 
 bool	SetDataBufferOffset ( UInt64 newDataBufferOffset );
 UInt64	GetDataBufferOffset ( void );
 
 bool	SetTimeoutDuration ( UInt32 timeoutValue );
 UInt32	GetTimeoutDuration ( void );
 
 bool	SetTaskCompletionCallback ( SCSITaskCompletion newCallback );
 void	TaskCompletedNotification ( void );
 
 bool	SetServiceResponse ( SCSIServiceResponse serviceResponse );
 SCSIServiceResponse GetServiceResponse ( void );
 
 // Set the auto sense data that was returned for the SCSI Task.  According
 // to the SAM-2 rev 13 specification section 5.6.4.1 "Autosense", if the
 // protocol and logical unit support autosense, a device server will only 
 // return autosense data in response to command completion with a CHECK 
 // CONDITION status.
 // A return value of true indicates that the data was copied to the member 
 // sense data structure, false indicates that the data could not be saved.
 bool	SetAutoSenseData ( SCSI_Sense_Data * senseData, UInt8 senseDataSize );
 
 bool	SetAutoSenseDataBuffer ( SCSI_Sense_Data *	senseData,
 UInt8				senseDataSize,
 task_t				task );
 
 // Get the auto sense data that was returned for the SCSI Task.  A return 
 // value of true indicates that valid auto sense data has been returned in 
 // the receivingBuffer.
 // A return value of false indicates that there is no auto sense data for 
 // this SCSI Task, and the receivingBuffer does not have valid data.
 // Since the SAM-2 specification only requires autosense data to be returned 
 // when a command completes with a CHECK CONDITION status, the autosense
 // data should only retrieved when the task status is 
 // kSCSITaskStatus_CHECK_CONDITION.
 // If the receivingBuffer is NULL, this routine will return whether the 
 // autosense data is valid without copying it to the receivingBuffer.
 bool	GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer, UInt8 senseDataSize );
 UInt8	GetAutoSenseDataSize ( void );
 
 // These are used by the SCSI Protocol Layer object for storing and
 // retrieving a reference number that is specific to that protocol such
 // as a Task Tag.
 bool	SetProtocolLayerReference ( void * newReferenceValue );
 void *	GetProtocolLayerReference ( void );
 
 // These are used by the SCSI Application Layer object for storing and
 // retrieving a reference number that is specific to that client.
 bool	SetApplicationLayerReference ( void * newReferenceValue );
 void *	GetApplicationLayerReference ( void );
 
 // These methods are only for the SCSI Protocol Layer to set the command
 // execution mode of the command.  There currently are two modes, standard
 // command execution for executing the command for which the task was 
 // created, and the autosense command execution mode for executing the 
 // Request Sense command for retrieving sense data.
 bool				SetTaskExecutionMode ( SCSITaskMode newTaskMode );
 SCSITaskMode		GetTaskExecutionMode ( void );
 
 bool				IsAutosenseRequested ( void );
 
 // This method is used only by the SCSI Protocol Layer to set the
 // state of the auto sense data when the REQUEST SENSE command is
 // explicitly sent to the device.	
 bool				SetAutosenseIsValid ( bool newAutosenseState );
 
 UInt8				GetAutosenseCommandDescriptorBlockSize ( void );
 
 bool				GetAutosenseCommandDescriptorBlock ( 
 SCSICommandDescriptorBlock * cdbData );
 
 UInt8				GetAutosenseDataTransferDirection ( void );
 
 UInt64				GetAutosenseRequestedDataTransferCount ( void );
 
 bool				SetAutosenseRealizedDataCount ( 
 UInt64 realizedTransferCountInBytes );
 UInt64				GetAutosenseRealizedDataCount ( void );
 
 IOMemoryDescriptor *	GetAutosenseDataBuffer ( void );
 
 bool				SetAutosenseCommand (
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5 );
 
 // These are the methods used for adding and removing the SCSI Task object
 // to a queue.  These are mainly for use by the SCSI Protocol Layer, but can
 // be used by the SCSI Application Layer if the task is currently not active
 // (Not active meaing that the Task state is either kSCSITaskState_NEW_TASK
 // or kSCSITaskState_ENDED).
 
 // This method queues the specified Task after this one
 void	EnqueueFollowingSCSITask ( SCSITask * followingTask );
 
 // Returns the pointer to the SCSI Task that is queued after
 // this one.  Returns NULL if one is not currently queued.
 SCSITask * GetFollowingSCSITask ( void );
 
 // Returns the pointer to the SCSI Task that is queued after
 // this one and removes it from the queue.  Returns NULL if 
 // one is not currently queued.
 SCSITask * DequeueFollowingSCSITask ( void );
 
 // Returns the pointer to the SCSI Task that is queued after
 // this one and removes it from the queue.  Returns NULL if 
 // one is not currently queued.  After dequeueing the following
 // Task, the specified newFollowingTask will be enqueued after this
 // task.
 SCSITask * ReplaceFollowingSCSITask ( SCSITask * newFollowingTask );
 
 private:
 
 // Space reserved for future expansion.
 OSMetaClassDeclareReservedUnused ( SCSITask, 1 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 2 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 3 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 4 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 5 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 6 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 7 );
 OSMetaClassDeclareReservedUnused ( SCSITask, 8 );
 
 };
 
 
 
 
 //�����������������������������������������������������������������������������
 //	� init - Initializes the object									   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::init ( void )
 {
 
 bool	result = false;
 
 require ( super::init ( ), ErrorExit );
 
 // Clear the owner here since it should be set when the object
 // is instantiated and never reset.
 fOwner					= NULL;
 fAutosenseDescriptor 	= NULL;
 
 // Set this task to the default task state.  
 fTaskState = kSCSITaskState_NEW_TASK;
 
 // Reset all the task's fields to their defaults.
 result = ResetForNewTask ( );
 
 
 ErrorExit:
 
 
 return result;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� free - Called to free any resources allocated					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 void
 SCSITask::free ( void )
 {
 
 if ( fOwner != NULL )
 {
 fOwner->release ( );
 }
 
 if ( fAutosenseDescriptor != NULL )
 {
 
 fAutosenseDescriptor->release ( );
 fAutosenseDescriptor = NULL;
 
 }
 
 if ( ( fAutosenseTaskMap == kernel_task ) && ( fAutoSenseData != NULL ) )
 {
 
 IOFree ( fAutoSenseData, fAutoSenseDataSize );
 fAutoSenseData = NULL;
 
 }
 
 super::free ( );
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� ResetForNewTask - Utility method to reset the object so that it may be
 //						used for a new Task. This method will return true if
 //						the reset was successful and false if it failed because
 //						it represents an active task				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::ResetForNewTask ( void )
 {
 
 bool				result = false;
 SCSI_Sense_Data *	buffer = NULL;
 
 // If this is a pending task, do not allow it to be reset until
 // it has completed.
 require ( ( IsTaskActive ( ) == false ), ErrorExit );
 
 fTaskAttribute 					= kSCSITask_SIMPLE;
 fTaskTagIdentifier				= kSCSIUntaggedTaskIdentifier;
 
 fTaskState 						= kSCSITaskState_NEW_TASK;
 fTaskStatus						= kSCSITaskStatus_GOOD;
 fLogicalUnitNumber				= 0;	
 
 bzero ( &fCommandDescriptorBlock, kSCSICDBSize_Maximum );
 
 fCommandSize 					= 0;
 fTransferDirection 				= 0;
 fDataBuffer						= NULL;
 fDataBufferOffset 				= 0;
 fRequestedByteCountOfTransfer	= 0;
 fRealizedByteCountOfTransfer	= 0;
 
 fTimeoutDuration				= 0;
 fCompletionCallback				= NULL;
 fServiceResponse				= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
 
 fNextTaskInQueue				= NULL;
 
 fProtocolLayerReference			= NULL;
 fApplicationLayerReference		= NULL;
 
 // Autosense member variables
 fAutosenseDataRequested			= false;
 fAutosenseCDBSize				= 0;
 fAutoSenseDataIsValid			= false;
 
 bzero ( &fAutosenseCDB, kSCSICDBSize_Maximum );
 
 if ( fAutoSenseData == NULL )
 {
 
 fAutoSenseDataSize = sizeof ( SCSI_Sense_Data );
 buffer = ( SCSI_Sense_Data * ) IOMalloc ( fAutoSenseDataSize );
 require_nonzero ( buffer, ErrorExit );
 bzero ( buffer, fAutoSenseDataSize );
 result = SetAutoSenseDataBuffer ( buffer, fAutoSenseDataSize, kernel_task );
 require ( result, ErrorExit );
 
 }
 
 fAutoSenseRealizedByteCountOfTransfer = 0;
 result = true;
 
 
 ErrorExit:
 
 
 return result;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetTaskOwner - Utility method for setting the OSObject that owns
 //					 the instantiation of the SCSI Task				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTaskOwner ( OSObject * taskOwner )
 {
 
 if ( fOwner != NULL )
 {
 
 // If this already has an owner, release
 // the retain on that one.
 fOwner->release ( );
 
 }
 
 fOwner = taskOwner;
 fOwner->retain ( );
 
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaskOwner - Utility method for retreiving the OSObject that owns the
 //					 instantiation of the SCSI Task					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 OSObject *
 SCSITask::GetTaskOwner ( void )
 {
 
 check ( fOwner );
 return fOwner;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaskOwner - Utility method to check if this task represents an active.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::IsTaskActive ( void )
 {
 
 // If the state of this task is either new or it is an ended task,
 // return false since this does not qualify as active.
 if ( ( fTaskState == kSCSITaskState_NEW_TASK ) ||
 ( fTaskState == kSCSITaskState_ENDED ) )
 {
 return false;
 }
 
 // If the task is in any other state, it is considered active.	
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetLogicalUnitNumber - 	Utility method for setting the Logical Unit
 //								Number for which this Task is intended.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetLogicalUnitNumber ( UInt8 newLUN )
 {
 
 fLogicalUnitNumber = newLUN;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetLogicalUnitNumber - 	Utility method for getting the Logical Unit
 //								Number for which this Task is intended.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt8
 SCSITask::GetLogicalUnitNumber( void )
 {
 return fLogicalUnitNumber;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetTaskAttribute - Sets the SCSITaskAttribute to the new value.  [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::SetTaskAttribute ( SCSITaskAttribute newAttributeValue )
 {
 
 fTaskAttribute = newAttributeValue;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaskAttribute - Gets the SCSITaskAttribute. 				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITaskAttribute	
 SCSITask::GetTaskAttribute ( void )
 {
 return fTaskAttribute;
 }
 
 //�����������������������������������������������������������������������������
 //	� SetTaggedTaskIdentifier - Sets the SCSITaggedTaskIdentifier.	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTaggedTaskIdentifier ( SCSITaggedTaskIdentifier newTag )
 {
 fTaskTagIdentifier = newTag;
 return true;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaggedTaskIdentifier - Gets the SCSITaggedTaskIdentifier. 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITaggedTaskIdentifier 
 SCSITask::GetTaggedTaskIdentifier ( void )
 {
 return fTaskTagIdentifier;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetTaskState - Sets the SCSITaskState to the new value. 		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTaskState ( SCSITaskState newTaskState )
 {
 fTaskState = newTaskState;
 return true;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaskState - Gets the SCSITaskState. 						   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITaskState	
 SCSITask::GetTaskState ( void )
 {
 return fTaskState;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetTaskStatus - Sets the SCSITaskStatus to the new value. 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTaskStatus ( SCSITaskStatus newTaskStatus )
 {
 
 fTaskStatus = newTaskStatus;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaskStatus - Gets the SCSITaskStatus.					 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITaskStatus	
 SCSITask::GetTaskStatus ( void )
 {
 return fTaskStatus;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetCommandDescriptorBlock - Populate the 6 Byte Command Descriptor Block
 //																 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool 
 SCSITask::SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5 )
 {
 
 fCommandDescriptorBlock[0] = cdbByte0;
 fCommandDescriptorBlock[1] = cdbByte1;
 fCommandDescriptorBlock[2] = cdbByte2;
 fCommandDescriptorBlock[3] = cdbByte3;
 fCommandDescriptorBlock[4] = cdbByte4;
 fCommandDescriptorBlock[5] = cdbByte5;
 fCommandDescriptorBlock[6] = 0x00;
 fCommandDescriptorBlock[7] = 0x00;
 fCommandDescriptorBlock[8] = 0x00;
 fCommandDescriptorBlock[9] = 0x00;
 fCommandDescriptorBlock[10] = 0x00;
 fCommandDescriptorBlock[11] = 0x00;
 fCommandDescriptorBlock[12] = 0x00;
 fCommandDescriptorBlock[13] = 0x00;
 fCommandDescriptorBlock[14] = 0x00;
 fCommandDescriptorBlock[15] = 0x00;
 
 fCommandSize = kSCSICDBSize_6Byte;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetCommandDescriptorBlock - Populate the 10 Byte Command Descriptor Block
 //																 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool 
 SCSITask::SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5,
 UInt8			cdbByte6,
 UInt8			cdbByte7,
 UInt8			cdbByte8,
 UInt8			cdbByte9 )
 {
 
 fCommandDescriptorBlock[0] = cdbByte0;
 fCommandDescriptorBlock[1] = cdbByte1;
 fCommandDescriptorBlock[2] = cdbByte2;
 fCommandDescriptorBlock[3] = cdbByte3;
 fCommandDescriptorBlock[4] = cdbByte4;
 fCommandDescriptorBlock[5] = cdbByte5;
 fCommandDescriptorBlock[6] = cdbByte6;
 fCommandDescriptorBlock[7] = cdbByte7;
 fCommandDescriptorBlock[8] = cdbByte8;
 fCommandDescriptorBlock[9] = cdbByte9;
 fCommandDescriptorBlock[10] = 0x00;
 fCommandDescriptorBlock[11] = 0x00;
 fCommandDescriptorBlock[12] = 0x00;
 fCommandDescriptorBlock[13] = 0x00;
 fCommandDescriptorBlock[14] = 0x00;
 fCommandDescriptorBlock[15] = 0x00;
 
 fCommandSize = kSCSICDBSize_10Byte;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetCommandDescriptorBlock - Populate the 12 Byte Command Descriptor Block
 //																 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool 
 SCSITask::SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5,
 UInt8			cdbByte6,
 UInt8			cdbByte7,
 UInt8			cdbByte8,
 UInt8			cdbByte9,
 UInt8			cdbByte10,
 UInt8			cdbByte11 )
 {
 
 fCommandDescriptorBlock[0] = cdbByte0;
 fCommandDescriptorBlock[1] = cdbByte1;
 fCommandDescriptorBlock[2] = cdbByte2;
 fCommandDescriptorBlock[3] = cdbByte3;
 fCommandDescriptorBlock[4] = cdbByte4;
 fCommandDescriptorBlock[5] = cdbByte5;
 fCommandDescriptorBlock[6] = cdbByte6;
 fCommandDescriptorBlock[7] = cdbByte7;
 fCommandDescriptorBlock[8] = cdbByte8;
 fCommandDescriptorBlock[9] = cdbByte9;
 fCommandDescriptorBlock[10] = cdbByte10;
 fCommandDescriptorBlock[11] = cdbByte11;
 fCommandDescriptorBlock[12] = 0x00;
 fCommandDescriptorBlock[13] = 0x00;
 fCommandDescriptorBlock[14] = 0x00;
 fCommandDescriptorBlock[15] = 0x00;
 
 fCommandSize = kSCSICDBSize_12Byte;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetCommandDescriptorBlock - Populate the 16 Byte Command Descriptor Block
 //																 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool 
 SCSITask::SetCommandDescriptorBlock ( 
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5,
 UInt8			cdbByte6,
 UInt8			cdbByte7,
 UInt8			cdbByte8,
 UInt8			cdbByte9,
 UInt8			cdbByte10,
 UInt8			cdbByte11,
 UInt8			cdbByte12,
 UInt8			cdbByte13,
 UInt8			cdbByte14,
 UInt8			cdbByte15 )
 {
 
 fCommandDescriptorBlock[0] = cdbByte0;
 fCommandDescriptorBlock[1] = cdbByte1;
 fCommandDescriptorBlock[2] = cdbByte2;
 fCommandDescriptorBlock[3] = cdbByte3;
 fCommandDescriptorBlock[4] = cdbByte4;
 fCommandDescriptorBlock[5] = cdbByte5;
 fCommandDescriptorBlock[6] = cdbByte6;
 fCommandDescriptorBlock[7] = cdbByte7;
 fCommandDescriptorBlock[8] = cdbByte8;
 fCommandDescriptorBlock[9] = cdbByte9;
 fCommandDescriptorBlock[10] = cdbByte10;
 fCommandDescriptorBlock[11] = cdbByte11;
 fCommandDescriptorBlock[12] = cdbByte12;
 fCommandDescriptorBlock[13] = cdbByte13;
 fCommandDescriptorBlock[14] = cdbByte14;
 fCommandDescriptorBlock[15] = cdbByte15;
 
 fCommandSize = kSCSICDBSize_16Byte;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetCommandDescriptorBlockSize - Gets the Command Descriptor Block size.
 //																 	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt8
 SCSITask::GetCommandDescriptorBlockSize ( void )
 {
 return fCommandSize;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetCommandDescriptorBlock - Gets the Command Descriptor Block.   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::GetCommandDescriptorBlock ( SCSICommandDescriptorBlock * cdbData )
 {
 
 bcopy ( fCommandDescriptorBlock,
 cdbData,
 sizeof ( SCSICommandDescriptorBlock ) );
 
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetDataTransferDirection - Sets the data transfer direction.	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetDataTransferDirection ( UInt8 newDataTransferDirection )
 {
 
 fTransferDirection = newDataTransferDirection;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetDataTransferDirection - Gets the data transfer direction.	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt8
 SCSITask::GetDataTransferDirection ( void )
 {
 return fTransferDirection;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetRequestedDataTransferCount - 	Sets the requested data transfer count
 //										in bytes.					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::SetRequestedDataTransferCount ( UInt64 requestedTransferCountInBytes )
 {
 
 fRequestedByteCountOfTransfer = requestedTransferCountInBytes;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetRequestedDataTransferCount - 	Gets the requested data transfer count
 //										in bytes.					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt64
 SCSITask::GetRequestedDataTransferCount ( void )
 {
 return fRequestedByteCountOfTransfer;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetRealizedDataTransferCount - Sets the realized data transfer count
 //									 in bytes.						   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes )
 {
 
 fRealizedByteCountOfTransfer = realizedTransferCountInBytes;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetRealizedDataTransferCount - Gets the realized data transfer count
 //									 in bytes.						   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt64
 SCSITask::GetRealizedDataTransferCount ( void )
 {
 return fRealizedByteCountOfTransfer;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetDataBuffer - Sets the data transfer buffer.				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetDataBuffer ( IOMemoryDescriptor * newDataBuffer )
 {
 
 fDataBuffer = newDataBuffer;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetDataBuffer - Gets the data transfer buffer.				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 IOMemoryDescriptor *
 SCSITask::GetDataBuffer ( void )
 {
 
 return fDataBuffer;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetDataBufferOffset - Sets the data transfer buffer offset.	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetDataBufferOffset ( UInt64 newDataBufferOffset )
 {
 
 fDataBufferOffset = newDataBufferOffset;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetDataBufferOffset - Gets the data transfer buffer offset.	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt64
 SCSITask::GetDataBufferOffset ( void )
 {
 return fDataBufferOffset;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetTimeoutDuration - 	Sets the command timeout value in milliseconds.
 //							Timeout values of zero indicate the largest
 //							possible timeout on that transport.		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTimeoutDuration ( UInt32 timeoutValue )
 {
 
 fTimeoutDuration = timeoutValue;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTimeoutDuration - 	Gets the command timeout value in milliseconds.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt32
 SCSITask::GetTimeoutDuration ( void )
 {
 return fTimeoutDuration;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetTaskCompletionCallback - Sets the command completion routine.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTaskCompletionCallback ( SCSITaskCompletion newCallback )
 {
 
 fCompletionCallback = newCallback;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� TaskCompletedNotification - Calls the command completion routine.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 void
 SCSITask::TaskCompletedNotification ( void )
 {
 fCompletionCallback ( this );
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetServiceResponse - Sets the SCSIServiceResponse.			   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetServiceResponse ( SCSIServiceResponse serviceResponse )
 {
 
 fServiceResponse = serviceResponse;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetServiceResponse - Gets the SCSIServiceResponse.			   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSIServiceResponse
 SCSITask::GetServiceResponse ( void )
 {
 return fServiceResponse;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetAutoSenseDataBuffer - Sets the auto sense data buffer.		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::SetAutoSenseDataBuffer ( SCSI_Sense_Data * 	senseData,
 UInt8				senseDataSize,
 task_t				task )
 {
 
 // Release any old memory descriptors
 if ( fAutosenseDescriptor != NULL )
 {
 
 fAutosenseDescriptor->release ( );
 fAutosenseDescriptor = NULL;
 
 }
 
 // Release any old memory
 if ( ( fAutosenseTaskMap == kernel_task ) && ( fAutoSenseData != NULL ) )
 {
 
 IOFree ( fAutoSenseData, fAutoSenseDataSize );
 fAutoSenseData = NULL;
 
 }
 
 // Set the new memory
 fAutoSenseData			= senseData;
 fAutoSenseDataSize		= senseDataSize;
 fAutoSenseDataIsValid	= false;
 fAutosenseTaskMap		= task;
 
 fAutosenseDescriptor = IOMemoryDescriptor::withAddress (
 ( vm_address_t ) fAutoSenseData,
 fAutoSenseDataSize,
 kIODirectionIn,
 fAutosenseTaskMap );
 
 check ( fAutosenseDescriptor );
 
 return ( fAutosenseDescriptor != NULL ) ? true : false;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetAutoSenseData - Sets the auto sense data.					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::SetAutoSenseData ( SCSI_Sense_Data * senseData, UInt8 senseDataSize )
 {
 
 UInt8	size = min ( fAutoSenseDataSize, senseDataSize );
 
 require_nonzero ( size, Exit );
 require_nonzero ( fAutosenseDescriptor, Exit );
 
 fAutosenseDescriptor->writeBytes ( 0, senseData, size );
 fAutoSenseDataIsValid = true;
 
 
 Exit:
 
 
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutoSenseData - Gets the auto sense data.					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::GetAutoSenseData ( SCSI_Sense_Data *	receivingBuffer,
 UInt8				senseDataSize )
 {
 
 bool	result 	= false;
 UInt8	size	= 0;
 
 require ( fAutoSenseDataIsValid, Exit );
 require_nonzero_action ( receivingBuffer, Exit, result = fAutoSenseDataIsValid );
 require_nonzero ( fAutosenseDescriptor, Exit );
 
 size = min ( fAutoSenseDataSize, senseDataSize );
 require_nonzero ( size, Exit );
 
 // Copy the data, but don't overflow the buffer
 fAutosenseDescriptor->readBytes ( 0, receivingBuffer, size );
 result = true;
 
 
 Exit:
 
 
 return result;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutoSenseDataSize - Gets the auto sense data size.			   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt8
 SCSITask::GetAutoSenseDataSize ( void )
 {
 
 return fAutoSenseDataSize;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetProtocolLayerReference - Sets the protocol layer reference value.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::SetProtocolLayerReference ( void * newReferenceValue )
 {
 
 check ( newReferenceValue );
 fProtocolLayerReference = newReferenceValue;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetProtocolLayerReference - Gets the protocol layer reference value.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 void *
 SCSITask::GetProtocolLayerReference ( void )
 {
 
 check ( fProtocolLayerReference );
 return fProtocolLayerReference;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetApplicationLayerReference - Sets the application layer reference value.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool	
 SCSITask::SetApplicationLayerReference ( void * newReferenceValue )
 {
 
 check ( newReferenceValue );
 fApplicationLayerReference = newReferenceValue;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetApplicationLayerReference - Gets the application layer reference value.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 void *
 SCSITask::GetApplicationLayerReference ( void )
 {
 
 check ( fApplicationLayerReference );
 return fApplicationLayerReference;
 
 }
 
 
 #if 0
 #pragma mark -
 #pragma mark � SCSI Protocol Layer Mode methods
 #pragma mark -
 #endif
 
 
 //�����������������������������������������������������������������������������
 //	� SetTaskExecutionMode - Sets the SCSITaskMode.					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetTaskExecutionMode ( SCSITaskMode newTaskMode )
 {
 
 fTaskExecutionMode = newTaskMode;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetTaskExecutionMode - Gets the SCSITaskMode.					   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITaskMode
 SCSITask::GetTaskExecutionMode ( void )
 {
 return fTaskExecutionMode;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� IsAutosenseRequested - Reports whether autosense data is requested.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::IsAutosenseRequested ( void )
 {
 return fAutosenseDataRequested;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetAutosenseIsValid - Sets the auto sense validity flag.		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool				
 SCSITask::SetAutosenseIsValid ( bool newAutosenseState )
 {
 
 fAutoSenseDataIsValid = newAutosenseState;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutosenseCommandDescriptorBlockSize - Gets the auto sense
 //											   Command Descriptor Block Size.
 //																	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt8
 SCSITask::GetAutosenseCommandDescriptorBlockSize ( void )
 {
 return fAutosenseCDBSize;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutosenseCommandDescriptorBlock -	Gets the auto sense
 //											Command Descriptor Block.  [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::GetAutosenseCommandDescriptorBlock ( SCSICommandDescriptorBlock * cdbData )
 {
 
 bcopy ( &fAutosenseCDB, cdbData, sizeof ( SCSICommandDescriptorBlock ) );
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutosenseDataTransferDirection -	Gets the auto sense data transfer
 //											direction. 				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt8
 SCSITask::GetAutosenseDataTransferDirection ( void )
 {
 return kSCSIDataTransfer_FromTargetToInitiator;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutosenseRequestedDataTransferCount -	Gets the auto sense requested
 //												data transfer count.   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt64
 SCSITask::GetAutosenseRequestedDataTransferCount ( void )
 {
 return sizeof ( SCSI_Sense_Data );
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetAutosenseRealizedDataCount -	Sets the auto sense realized data
 //										transfer count.				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetAutosenseRealizedDataCount ( UInt64 realizedTransferCountInBytes )
 {
 
 fAutoSenseRealizedByteCountOfTransfer = realizedTransferCountInBytes;
 return true;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutosenseRealizedDataCount -	Gets the auto sense realized data
 //										transfer count.				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 UInt64
 SCSITask::GetAutosenseRealizedDataCount ( void )
 {
 return fAutoSenseRealizedByteCountOfTransfer;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetAutosenseDataBuffer -	Gets the auto sense data buffer.	   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 IOMemoryDescriptor *
 SCSITask::GetAutosenseDataBuffer ( void )
 {
 return fAutosenseDescriptor;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� SetAutosenseCommand -	Sets the auto sense command.	 		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 bool
 SCSITask::SetAutosenseCommand (
 UInt8			cdbByte0,
 UInt8			cdbByte1,
 UInt8			cdbByte2,
 UInt8			cdbByte3,
 UInt8			cdbByte4,
 UInt8			cdbByte5 )
 {
 
 fAutosenseCDB[0] = cdbByte0;
 fAutosenseCDB[1] = cdbByte1;
 fAutosenseCDB[2] = cdbByte2;
 fAutosenseCDB[3] = cdbByte3;
 fAutosenseCDB[4] = cdbByte4;
 fAutosenseCDB[5] = cdbByte5;
 fAutosenseCDB[6] = 0x00;
 fAutosenseCDB[7] = 0x00;
 fAutosenseCDB[8] = 0x00;
 fAutosenseCDB[9] = 0x00;
 fAutosenseCDB[10] = 0x00;
 fAutosenseCDB[11] = 0x00;
 fAutosenseCDB[12] = 0x00;
 fAutosenseCDB[13] = 0x00;
 fAutosenseCDB[14] = 0x00;
 fAutosenseCDB[15] = 0x00;
 
 fAutosenseCDBSize = kSCSICDBSize_6Byte;
 fAutosenseDataRequested = true;
 
 return true;
 
 }
 
 
 #if 0
 #pragma mark -
 #pragma mark � SCSI Task Queue Management Methods
 #pragma mark -
 #endif
 
 // These are the methods used for adding and removing the SCSI Task object
 // to a queue.  These are mainly for use by the SCSI Protocol Layer, but can be
 // used by the SCSI Application Layer if the task is currently not active (the
 // Task state is kSCSITaskState_NEW_TASK or kSCSITaskState_ENDED).
 
 //�����������������������������������������������������������������������������
 //	� EnqueueFollowingSCSITask - Enqueues the specified Task after this one.
 //															 		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 void
 SCSITask::EnqueueFollowingSCSITask ( SCSITask * followingTask )
 {
 fNextTaskInQueue = followingTask;
 }
 
 
 //�����������������������������������������������������������������������������
 //	� GetFollowingSCSITask - Returns the pointer to the SCSI Task that is
 //							 queued after this one. Returns NULL if one is not
 //							 currently queued.				 		   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITask *
 SCSITask::GetFollowingSCSITask ( void )
 {
 
 return fNextTaskInQueue;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� DequeueFollowingSCSITask - Returns the pointer to the SCSI Task that is
 //							 	 queued after this one. Returns NULL if one is
 //							 	 not currently queued.				   [PUBLIC]
 //�����������������������������������������������������������������������������
 
 SCSITask *
 SCSITask::DequeueFollowingSCSITask ( void )
 {
 
 SCSITask *	returnTask;
 
 returnTask 			= fNextTaskInQueue;
 fNextTaskInQueue 	= NULL;
 
 return returnTask;
 
 }
 
 
 //�����������������������������������������������������������������������������
 //	� ReplaceFollowingSCSITask - Returns the pointer to the SCSI Task that is
 //								 queued after this one and removes it from the
 //								 queue.  Returns NULL if one is not currently
 //								 queued.  After dequeueing the following Task,
 //								 the specified newFollowingTask will be
 //								 enqueued after this task.			   [PUBLIC]
 //�����������������������������������������������������������������������������
 SCSITask *
 SCSITask::ReplaceFollowingSCSITask ( SCSITask * newFollowingTask )
 {
 
 SCSITask *	returnTask = NULL;
 
 returnTask 			= fNextTaskInQueue;
 fNextTaskInQueue 	= newFollowingTask;
 
 return returnTask;
 
 }
 */

//
// an interface made from the real undocumented definition, see above for the full definition
//
class SCSITask
{
	
public:
	
	// Utility methods for setting and retreiving the Object that owns the
	// instantiation of the SCSI Task
	bool				SetTaskOwner ( OSObject	* taskOwner );
	OSObject *			GetTaskOwner ( void );
	
    // Utility method to reset the object so that it may be used for a new
	// Task.  This method will return true if the reset was successful
	// and false if it failed because it represents an active task.
	bool				ResetForNewTask ( void );
	
	// Utility method to check if this task represents an active.
	bool				IsTaskActive ( void );
	
	// Utility Methods for managing the Logical Unit Number for which this Task 
	// is intended.
	bool				SetLogicalUnitNumber ( UInt8 newLUN );
	UInt8				GetLogicalUnitNumber ( void );
	
	// The following methods are used to set and to get the value of the
	// task's attributes.  The set methods all return a bool which indicates
	// whether the attribute was successfully set.  The set methods will return
	// true if the attribute was updated to the new value and false if it was
	// not.  A false return value usually indicates that the task is active
	// and the attribute that was requested to be changed, can not be changed
	// once a task is active.
	// The get methods will always return the current value of the attribute
	// regardless of whether it has been previously set and regardless of
	// whether or not the task is active.	
	bool				SetTaskAttribute ( 
                                          SCSITaskAttribute newAttributeValue );
	SCSITaskAttribute	GetTaskAttribute ( void );
    
	bool				SetTaggedTaskIdentifier ( 
                                                 SCSITaggedTaskIdentifier newTag );
	SCSITaggedTaskIdentifier GetTaggedTaskIdentifier ( void );
    
	bool				SetTaskState ( SCSITaskState newTaskState );
	SCSITaskState		GetTaskState ( void );
	
	// Accessor methods for getting and setting that status of the Task.	
	bool				SetTaskStatus ( SCSITaskStatus newTaskStatus );
	SCSITaskStatus		GetTaskStatus ( void );
	
    // Accessor functions for setting the properties of the Task
    
    // Methods for populating the Command Descriptor Block.  Individual methods
    // are used instead of having a single method so that the CDB size is
    // automatically known.
	// Populate the 6 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock ( 
                                       UInt8			cdbByte0,
                                       UInt8			cdbByte1,
                                       UInt8			cdbByte2,
                                       UInt8			cdbByte3,
                                       UInt8			cdbByte4,
                                       UInt8			cdbByte5 );
	
	// Populate the 10 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock ( 
                                       UInt8			cdbByte0,
                                       UInt8			cdbByte1,
                                       UInt8			cdbByte2,
                                       UInt8			cdbByte3,
                                       UInt8			cdbByte4,
                                       UInt8			cdbByte5,
                                       UInt8			cdbByte6,
                                       UInt8			cdbByte7,
                                       UInt8			cdbByte8,
                                       UInt8			cdbByte9 );
	
	// Populate the 12 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock ( 
                                       UInt8			cdbByte0,
                                       UInt8			cdbByte1,
                                       UInt8			cdbByte2,
                                       UInt8			cdbByte3,
                                       UInt8			cdbByte4,
                                       UInt8			cdbByte5,
                                       UInt8			cdbByte6,
                                       UInt8			cdbByte7,
                                       UInt8			cdbByte8,
                                       UInt8			cdbByte9,
                                       UInt8			cdbByte10,
                                       UInt8			cdbByte11 );
	
	// Populate the 16 Byte Command Descriptor Block
	bool 	SetCommandDescriptorBlock ( 
                                       UInt8			cdbByte0,
                                       UInt8			cdbByte1,
                                       UInt8			cdbByte2,
                                       UInt8			cdbByte3,
                                       UInt8			cdbByte4,
                                       UInt8			cdbByte5,
                                       UInt8			cdbByte6,
                                       UInt8			cdbByte7,
                                       UInt8			cdbByte8,
                                       UInt8			cdbByte9,
                                       UInt8			cdbByte10,
                                       UInt8			cdbByte11,
                                       UInt8			cdbByte12,
                                       UInt8			cdbByte13,
                                       UInt8			cdbByte14,
                                       UInt8			cdbByte15 );
	
	UInt8	GetCommandDescriptorBlockSize ( void );
	
	// This will always return a 16 Byte CDB.  If the Protocol Layer driver
	// does not support 16 Byte CDBs, it will have to create a local 
	// SCSICommandDescriptorBlock variable to get the CDB data and then
	// transfer the needed bytes from there.
	bool	GetCommandDescriptorBlock ( 
                                       SCSICommandDescriptorBlock * cdbData );
	
	// Set up the control information for the transfer, including
	// the transfer direction and the number of bytes to transfer.
	bool	SetDataTransferDirection ( UInt8 newDataTransferDirection );
	UInt8	GetDataTransferDirection ( void );
	
	bool	SetRequestedDataTransferCount ( UInt64 requestedTransferCountInBytes );
	UInt64	GetRequestedDataTransferCount ( void );
	
	bool	SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes );
	UInt64	GetRealizedDataTransferCount ( void );
	
	bool	SetDataBuffer ( IOMemoryDescriptor * newDataBuffer );
	IOMemoryDescriptor * GetDataBuffer ( void );
	
	bool	SetDataBufferOffset ( UInt64 newDataBufferOffset );
	UInt64	GetDataBufferOffset ( void );
	
	bool	SetTimeoutDuration ( UInt32 timeoutValue );
	UInt32	GetTimeoutDuration ( void );
	
	bool	SetTaskCompletionCallback ( SCSITaskCompletion newCallback );
	void	TaskCompletedNotification ( void );
	
	bool	SetServiceResponse ( SCSIServiceResponse serviceResponse );
	SCSIServiceResponse GetServiceResponse ( void );
	
	// Set the auto sense data that was returned for the SCSI Task.  According
	// to the SAM-2 rev 13 specification section 5.6.4.1 "Autosense", if the
	// protocol and logical unit support autosense, a device server will only 
	// return autosense data in response to command completion with a CHECK 
	// CONDITION status.
	// A return value of true indicates that the data was copied to the member 
	// sense data structure, false indicates that the data could not be saved.
	bool	SetAutoSenseData ( SCSI_Sense_Data * senseData, UInt8 senseDataSize );
	
	bool	SetAutoSenseDataBuffer ( SCSI_Sense_Data *	senseData,
                                    UInt8				senseDataSize,
                                    task_t				task );
	
	// Get the auto sense data that was returned for the SCSI Task.  A return 
	// value of true indicates that valid auto sense data has been returned in 
	// the receivingBuffer.
	// A return value of false indicates that there is no auto sense data for 
	// this SCSI Task, and the receivingBuffer does not have valid data.
	// Since the SAM-2 specification only requires autosense data to be returned 
	// when a command completes with a CHECK CONDITION status, the autosense
	// data should only retrieved when the task status is 
	// kSCSITaskStatus_CHECK_CONDITION.
	// If the receivingBuffer is NULL, this routine will return whether the 
	// autosense data is valid without copying it to the receivingBuffer.
	bool	GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer, UInt8 senseDataSize );
	UInt8	GetAutoSenseDataSize ( void );
	
	// These are used by the SCSI Protocol Layer object for storing and
	// retrieving a reference number that is specific to that protocol such
	// as a Task Tag.
	bool	SetProtocolLayerReference ( void * newReferenceValue );
	void *	GetProtocolLayerReference ( void );
	
	// These are used by the SCSI Application Layer object for storing and
	// retrieving a reference number that is specific to that client.
	bool	SetApplicationLayerReference ( void * newReferenceValue );
	void *	GetApplicationLayerReference ( void );
	
	// These methods are only for the SCSI Protocol Layer to set the command
	// execution mode of the command.  There currently are two modes, standard
	// command execution for executing the command for which the task was 
	// created, and the autosense command execution mode for executing the 
	// Request Sense command for retrieving sense data.
	bool				SetTaskExecutionMode ( SCSITaskMode newTaskMode );
	SCSITaskMode		GetTaskExecutionMode ( void );
	
	bool				IsAutosenseRequested ( void );
	
	// This method is used only by the SCSI Protocol Layer to set the
	// state of the auto sense data when the REQUEST SENSE command is
	// explicitly sent to the device.	
	bool				SetAutosenseIsValid ( bool newAutosenseState );
	
	UInt8				GetAutosenseCommandDescriptorBlockSize ( void );
	
	bool				GetAutosenseCommandDescriptorBlock ( 
                                                            SCSICommandDescriptorBlock * cdbData );
	
	UInt8				GetAutosenseDataTransferDirection ( void );
	
	UInt64				GetAutosenseRequestedDataTransferCount ( void );
	
	bool				SetAutosenseRealizedDataCount ( 
                                                       UInt64 realizedTransferCountInBytes );
	UInt64				GetAutosenseRealizedDataCount ( void );
	
	IOMemoryDescriptor *	GetAutosenseDataBuffer ( void );
	
	bool				SetAutosenseCommand (
                                             UInt8			cdbByte0,
                                             UInt8			cdbByte1,
                                             UInt8			cdbByte2,
                                             UInt8			cdbByte3,
                                             UInt8			cdbByte4,
                                             UInt8			cdbByte5 );
	
};

//--------------------------------------------------------------------

const char*
DldSCSICommandToName(
    __in UInt8  command//cdb[0]
    )
{
    switch ( command ){
            
        case kSCSICmd_FORMAT_UNIT: // FORMAT UNIT
            return "kSCSICmd_FORMAT_UNIT";
            
        case kSCSICmd_LOAD_UNLOAD_MEDIUM: // cancelling SCSIOP_LOAD_UNLOAD might make NERO unresponsible while erasing CD
            return "kSCSICmd_LOAD_UNLOAD_MEDIUM";
            
        case kSCSICmd_WRITE_6://WRITE(6)
            return "kSCSICmd_WRITE_6";
            
        case kSCSICmd_WRITE_10:// WRITE (10)
            return "kSCSICmd_WRITE_10";
            
        case kSCSICmd_WRITE_12:
            return "kSCSICmd_WRITE_12";
            
        case kSCSICmd_WRITE_16:// 16 byte commands can't be sent via ATAPI
            return "kSCSICmd_WRITE_16";
            
        case kSCSICmd_WRITE_AND_VERIFY_10:// WRITE AND VERIFY (10)
            return "kSCSICmd_WRITE_AND_VERIFY_10";
            
        case kSCSICmd_WRITE_AND_VERIFY_12:
            return "kSCSICmd_WRITE_AND_VERIFY_12";
            
        case kSCSICmd_WRITE_AND_VERIFY_16:
            return "kSCSICmd_WRITE_AND_VERIFY_16";
            
        case kSCSICmd_SYNCHRONIZE_CACHE: // SYNCHRONIZE CACHE
            return "kSCSICmd_SYNCHRONIZE_CACHE";
            
        case kSCSICmd_SYNCHRONIZE_CACHE_16:
            return "kSCSICmd_SYNCHRONIZE_CACHE_16";
            
        case kSCSICmd_WRITE_BUFFER:// 0x3b
            return "kSCSICmd_WRITE_BUFFER";
            
        case kSCSICmd_SEND_DVD_STRUCTURE:// used to send structure description of the data which will be written to the disk
            return "kSCSICmd_SEND_DVD_STRUCTURE";
            
        case kSCSICmd_SEND_CUE_SHEET:// used to send structure desription of the information at Session-at-once write mode
            //
            // the MODE SELECT command is used to
            // set the new mode for the CD drive
            // for example to switch it from the 
            // current mode to raw mode before
            // raw read operation, so this command
            // is excluded from the controlled operations
            //
            //case SCSIOP_MODE_SELECT:   // MODE SELECT
            //case SCSIOP_MODE_SELECT10: // MODE SELECT (10)
            return "kSCSICmd_SEND_CUE_SHEET";
            
        case kSCSICmd_PLAY_AUDIO_10: // PLAY AUDIO (10)
            return "kSCSICmd_PLAY_AUDIO_10";
            
        case kSCSICmd_PLAY_AUDIO_12:
            return "kSCSICmd_PLAY_AUDIO_12";
            
        case kSCSICmd_PLAY_AUDIO_MSF:
            return "kSCSICmd_PLAY_AUDIO_MSF";
            
        case kSCSICmd_PLAY_AUDIO_TRACK_INDEX:
            return "kSCSICmd_PLAY_AUDIO_TRACK_INDEX";
            
        case kSCSICmd_PLAY_CD:
            return "kSCSICmd_PLAY_CD";
            
        case kSCSICmd_PLAY_RELATIVE_10:
            return "kSCSICmd_PLAY_RELATIVE_10";
            
        case kSCSICmd_PLAY_RELATIVE_12:
            //case SCSIOP_CLOSE_TRACK_SESSION: // cancelling CLOSE TRACK/SESSION damages burned CDs
            return "kSCSICmd_PLAY_RELATIVE_12";
            
        case kSCSICmd_BLANK: // BLANK
            return "kSCSICmd_BLANK";
            
        case kSCSICmd_READ_6:
            return "kSCSICmd_READ_6";
            
        case kSCSICmd_READ_10:
            return "kSCSICmd_READ_10";
            
        case kSCSICmd_READ_12:
            return "kSCSICmd_READ_12";
            
        case kSCSICmd_READ_16:
            return "kSCSICmd_READ_16";
            
        case kSCSICmd_READ_BUFFER:
            return "kSCSICmd_READ_BUFFER";
            
        case kSCSICmd_READ_CD:
            return "kSCSICmd_READ_CD";
            
        case kSCSICmd_READ_CD_MSF:
            return "kSCSICmd_READ_CD_MSF";
            
        case kSCSICmd_READ_DVD_STRUCTURE:
            return "kSCSICmd_READ_DVD_STRUCTURE";
            
        case kSCSICmd_READ_HEADER:
            return "kSCSICmd_READ_HEADER";
            
        case kSCSICmd_READ_TOC_PMA_ATIP:
            return "kSCSICmd_READ_TOC_PMA_ATIP";
            
        default: // Easy Media Creator additional commands: AD 52 5C 25 4A
            return "UNKNOWN_SCSI_COMMAND";
    }
}

void
DldLogSCSITask(
    __in SCSITaskIdentifier   request
    )
{
    
    SCSICommandDescriptorBlock	cdb;
    UInt8						commandLength;
    char                        procName[ 64 ];
    
    assert( kSCSICDBSize_Maximum == sizeof( cdb ) );
    
    DldSCSITaskGetCommandDescriptorBlock ( request, &cdb );
    commandLength = DldSCSITaskGetCommandDescriptorBlockSize( request );
    
    procName[ sizeof( procName ) - 0x1 ] = '\0';
    proc_selfname( procName, sizeof(procName) - 0x1 );// suppose that we are in the caller's context
    
    if ( commandLength == kSCSICDBSize_6Byte )
    {
        
        DLD_COMM_LOG( SCSI,
                      ( "%s : cdb = %02x:%02x:%02x:%02x:%02x:%02x : BSD proc[%u][%s]\n", DldSCSICommandToName( cdb[0] ),
                       cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5],
                       proc_selfpid(),
                       procName ) );
        
    }
    else if ( commandLength == kSCSICDBSize_10Byte )
    {
        
        DLD_COMM_LOG ( SCSI,
                       ( "%s : cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x : BSD proc[%u][%s]\n", DldSCSICommandToName( cdb[0] ),
                        cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8], cdb[9],
                        proc_selfpid(),
                        procName ) );
        
    }
    else if ( commandLength == kSCSICDBSize_12Byte )
    {
        
        DLD_COMM_LOG ( SCSI,
                       ( "%s : cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x : BSD proc[%u][%s]\n", DldSCSICommandToName( cdb[0] ),
                        cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
                        cdb[9], cdb[10], cdb[11],
                        proc_selfpid(),
                        procName ) );
        
    }
    else if ( commandLength == kSCSICDBSize_16Byte )
    {
        
        DLD_COMM_LOG ( SCSI,
                       ( "%s : cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x : BSD proc[%u][%s]\n", DldSCSICommandToName( cdb[0] ),
                        cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
                        cdb[9], cdb[10], cdb[11], cdb[12], cdb[13], cdb[14], cdb[15],
                        proc_selfpid(),
                        procName ) );
        
    } else {
        
        assert( !"SCSI: unknown command length" );
    }
    
}

//--------------------------------------------------------------------

bool
DldIsSCSITaskObject(
    __in SCSITaskIdentifier   request
)
{
    return ( NULL != ((OSObject*)request)->metaCast("SCSITask") );
}

//--------------------------------------------------------------------

//
// This will always return a 16 Byte CDB
//
bool  DldSCSITaskGetCommandDescriptorBlock( __in    SCSITaskIdentifier   request,
                                            __inout SCSICommandDescriptorBlock * cdbData )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetCommandDescriptorBlock( cdbData );
}

//--------------------------------------------------------------------

bool  DldSCSITaskSetServiceResponse( __inout SCSITaskIdentifier   request,
                                     __in SCSIServiceResponse serviceResponse )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->SetServiceResponse( serviceResponse );
}

//--------------------------------------------------------------------

bool DldSCSITaskSetTaskStatus( __inout SCSITaskIdentifier request,
                               __in    SCSITaskStatus newTaskStatus )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->SetTaskStatus( newTaskStatus );
}

//--------------------------------------------------------------------

void DldSCSITaskCompletedNotification ( __inout SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->TaskCompletedNotification();
}

//--------------------------------------------------------------------

//
// returned values are kSCSICDBSize_6Byte, kSCSICDBSize_10Byte, kSCSICDBSize_12Byte, kSCSICDBSize_16Byte
//
UInt8 DldSCSITaskGetCommandDescriptorBlockSize ( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetCommandDescriptorBlockSize();
}

//--------------------------------------------------------------------

bool DldSCSITaskSetTaskCompletionCallback ( __inout SCSITaskIdentifier request, __in SCSITaskCompletion newCallback )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->SetTaskCompletionCallback( newCallback );
}

//--------------------------------------------------------------------

IOMemoryDescriptor * DldSCSITaskGetDataBuffer ( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetDataBuffer();
}

//--------------------------------------------------------------------

SCSIServiceResponse DldSCSITaskGetServiceResponse( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetServiceResponse();
}

//--------------------------------------------------------------------

SCSITaskStatus DldSCSITaskGetTaskStatus( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetTaskStatus();
}

//--------------------------------------------------------------------

UInt64	DldSCSITaskGetRealizedDataTransferCount( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetRealizedDataTransferCount();
}

//--------------------------------------------------------------------

UInt64 DldSCSITaskGetRequestedDataTransferCount( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetRequestedDataTransferCount();
}

//--------------------------------------------------------------------

UInt64 DldSCSITaskGetDataBufferOffset( __in SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    return ((SCSITask*)request)->GetDataBufferOffset();
}

//--------------------------------------------------------------------

void DldSCSITaskCompleteAccessDenied( __inout SCSITaskIdentifier request )
{
    assert( DldIsSCSITaskObject( request ) );
    
	DldSCSITaskSetServiceResponse ( request, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
	DldSCSITaskSetTaskStatus ( request, kSCSITaskStatus_No_Status );
    //
	// The task has completed, execute the callback.
    //
	DldSCSITaskCompletedNotification ( request );
}

//--------------------------------------------------------------------

bool
DldSCSITaskIsMediaEjectionRequest( __in SCSITaskIdentifier request )
{
    SCSICommandDescriptorBlock   cdb;
    
    assert( kSCSICDBSize_Maximum == sizeof( cdb ) );
    
    DldSCSITaskGetCommandDescriptorBlock( request, &cdb );

            
    if( kSCSICmd_START_STOP_UNIT == cdb[0] ){
            //
            // Mac OS likes this command, it uses it for media ejection and loading
            // a description for the command can be found here http://en.wikipedia.org/wiki/SCSI_Start_Stop_Unit_Command
            // and we are particularly interested in 
            // LoEj (load/eject) and Start - these two bits are used together:
            //  00 - Stop motor
            //  01 - Start motor
            //  10 - Eject media
            //  11 - Load media
            //
        return( 0x2 == ( cdb[4] & 0x3 ) );
    }
    
    if( kSCSICmd_LOAD_UNLOAD_MEDIUM == cdb[0] )
        return true;
    
    return false;
}

//--------------------------------------------------------------------

dld_classic_rights_t 
DldSCSITaskGetCdbRequestedAccess( __in SCSITaskIdentifier request )
{
    SCSICommandDescriptorBlock   cdb;
    
    assert( kSCSICDBSize_Maximum == sizeof( cdb ) );
    
    DldSCSITaskGetCommandDescriptorBlock( request, &cdb );
    
    if( gGlobalSettings.logFlags.SCSI ){
        DldLogSCSITask( request );
    }
    
    switch (cdb[0]){
            
        case kSCSICmd_START_STOP_UNIT:
            //
            // Mac OS X likes this command, it uses it for media ejection and loading
            // a description for the command can be found here http://en.wikipedia.org/wiki/SCSI_Start_Stop_Unit_Command
            // and we are particularly interested in 
            // LoEj (load/eject) and Start - these two bits are used together:
            //  00 - Stop motor
            //  01 - Start motor
            //  10 - Eject media
            //  11 - Load media
            //
            if( 0x2 == ( cdb[4] & 0x3 ) )
                return DEVICE_EJECT_MEDIA;
            return 0x0;
            
        case kSCSICmd_FORMAT_UNIT: // FORMAT UNIT
            return DEVICE_DISK_FORMAT;
            
        case kSCSICmd_LOAD_UNLOAD_MEDIUM: // cancelling SCSIOP_LOAD_UNLOAD might make NERO unresponsible while erasing CD
            return DEVICE_EJECT_MEDIA;
            
        case kSCSICmd_WRITE_6://WRITE(6)
        case kSCSICmd_WRITE_10:// WRITE (10)
        case kSCSICmd_WRITE_12:
        case kSCSICmd_WRITE_16:// 16 byte commands can't be sent via ATAPI
        case kSCSICmd_WRITE_AND_VERIFY_10:// WRITE AND VERIFY (10)
        case kSCSICmd_WRITE_AND_VERIFY_12:
        case kSCSICmd_WRITE_AND_VERIFY_16:
        //case kSCSICmd_SYNCHRONIZE_CACHE: a false write, benign// SYNCHRONIZE CACHE
        case kSCSICmd_SYNCHRONIZE_CACHE_16:
        case kSCSICmd_WRITE_BUFFER:// 0x3b
        case kSCSICmd_SEND_DVD_STRUCTURE:// used to send structure description of the data which will be written to the disk
        case kSCSICmd_SEND_CUE_SHEET:// used to send structure desription of the information at Session-at-once write mode
            //
            // the MODE SELECT command is used to
            // set the new mode for the CD drive
            // for example to switch it from the 
            // current mode to raw mode before
            // raw read operation, so this command
            // is excluded from the controlled operations
            //
            //case SCSIOP_MODE_SELECT:   // MODE SELECT
            //case SCSIOP_MODE_SELECT10: // MODE SELECT (10)
            //DBG_PRINT_ERROR(( "DldSCSITaskGetCdbRequestedAccess: DEVICE_WRITE [%d]\n", cdb[0] ));
            return DEVICE_WRITE;
            
        case kSCSICmd_PLAY_AUDIO_10: // PLAY AUDIO (10)
        case kSCSICmd_PLAY_AUDIO_12:
        case kSCSICmd_PLAY_AUDIO_MSF:
        case kSCSICmd_PLAY_AUDIO_TRACK_INDEX:
        case kSCSICmd_PLAY_CD:
        case kSCSICmd_PLAY_RELATIVE_10:
        case kSCSICmd_PLAY_RELATIVE_12:
            //case SCSIOP_CLOSE_TRACK_SESSION: // cancelling CLOSE TRACK/SESSION damages burned CDs
            return DEVICE_PLAY_AUDIO_CD;
            
        case kSCSICmd_BLANK: // BLANK
            return DEVICE_DISK_FORMAT; // changed from DEVICE_DELETE, there is no any explanation for this except that the DL user manual says so
            
        case kSCSICmd_READ_6:
        case kSCSICmd_READ_10:
        case kSCSICmd_READ_12:
        case kSCSICmd_READ_16:
        case kSCSICmd_READ_BUFFER:
        case kSCSICmd_READ_CD:
        case kSCSICmd_READ_CD_MSF:
        case kSCSICmd_READ_DVD_STRUCTURE:
        case kSCSICmd_READ_HEADER:
        case kSCSICmd_READ_TOC_PMA_ATIP:
            return DEVICE_READ;
            
        default: // Easy Media Creator additional commands: AD 52 5C 25 4A
            //dprintf("SCSI CMD: %X\n",cdb[0]);
            break;
            
    }
    
    return 0;
}

//--------------------------------------------------------------------

#define super OSObject

OSDefineMetaClassAndStructors( DldSCSITask, OSObject )

//--------------------------------------------------------------------

//
// all DldSCSITask objects are linked in the list
//
DldSCSITask::DldSCSITaskListHead DldSCSITask::gSCSITaskListHead = {NULL};
vm_offset_t                      DldSCSITask::gCallbackOffset   = (vm_offset_t)(-1);
bool                             DldSCSITask::gStaticInit       = false;
IORWLock*                        DldSCSITask::rwLock            = NULL;
#if defined( DBG )
thread_t                         DldSCSITask::exclusiveThread   = NULL;
#endif//DBG

//--------------------------------------------------------------------

IOReturn
DldSCSITask::DldInitSCSITaskSubsystem()
{
    CIRCLEQ_INIT_WITH_TYPE( &DldSCSITask::gSCSITaskListHead, DldSCSITask );
    
    DldSCSITask::rwLock = IORWLockAlloc();
    assert( DldSCSITask::rwLock );
    if( !DldSCSITask::rwLock ){
        
        DBG_PRINT_ERROR(( "IORWLockAlloc() failed to allocate DldSCSITask::rwLock\n" ));
    }
    
    gDldDbgData.gSCSITaskListHead = (void*)&gSCSITaskListHead;
    
    DldSCSITask::gStaticInit = true;
    
    return KERN_SUCCESS;
}

//--------------------------------------------------------------------

void
DldSCSITask::DldFreeSCSITaskSubsystem()
{
    if( DldSCSITask::rwLock )
        IORWLockFree( DldSCSITask::rwLock );
}

//--------------------------------------------------------------------

void DldFakeSCSITaskCompletion( __in SCSITaskIdentifier completedTask )
{
    panic( "DldFakeSCSITaskCompletion is called!" );
}

//--------------------------------------------------------------------

DldSCSITask*  DldSCSITask::withSCSITaskIdentifier( __in SCSITaskIdentifier   request, __in DldIOService* dldService )
{
    DldSCSITask*  task;
    
    assert( preemption_enabled() );
    assert( gStaticInit );
    assert( DldIsSCSITaskObject( request ) );
    assert( NULL == DldSCSITask::GetReferencedSCSITask( request ) );
    
    if( !DldIsSCSITaskObject( request ) ){
        
        DBG_PRINT_ERROR(( "request is of %s type which can't be processed\n", request->getMetaClass()->getClassName() ));
        return NULL;
    }
    
    //
    // get an offset for the callback, do not warry about a concurrency,
    // this is not an issue here as the concurrent requests are quit rare
    // and don't harm as the behaviour is idempotent at any point
    //
    if( (vm_offset_t)(-1) == gCallbackOffset ){
        
        unsigned int       requestClassSize;
        SCSITaskIdentifier requestCopy;
        
        
        requestClassSize = request->getMetaClass()->getClassSize();
        assert( requestClassSize );
        
        requestCopy = (SCSITaskIdentifier)IOMalloc( requestClassSize );
        if( !requestCopy ){
            
            DBG_PRINT_ERROR(("IOMalloc(%u) failed\n", requestClassSize));
            return NULL;
        }
        
        memcpy( requestCopy, request, requestClassSize );
        
        //
        // set the fake completion
        //
        if( !DldSCSITaskSetTaskCompletionCallback( request, DldFakeSCSITaskCompletion ) ){
            
            DBG_PRINT_ERROR(("DldSCSITaskSetTaskCompletionCallback() failed\n"));
            IOFree( requestCopy, requestClassSize );
            return NULL;
        }
        
        //
        // find the fake completion by a simple memory scan
        //
        for( int i = 0x0; i < requestClassSize/sizeof( SCSITaskCompletion ); ++i ){
            
            if( ((SCSITaskCompletion*)request)[i] != DldFakeSCSITaskCompletion )
                continue;
            
            gCallbackOffset = (vm_offset_t)i*sizeof(SCSITaskCompletion);
            break;
        }// end for
        
        if( (vm_offset_t)(-1) == gCallbackOffset ){
            
            assert( !"gCallbackOffset was not set!" );
            DBG_PRINT_ERROR(("gCallbackOffset was not set!"));
            IOFree( requestCopy, requestClassSize );
            return NULL;
        }
        
        //
        // restore the original callback
        //
        DldSCSITaskSetTaskCompletionCallback( request, *(SCSITaskCompletion*)( (vm_offset_t)requestCopy+gCallbackOffset ) );
        
        //
        // free the copy
        //
        IOFree( requestCopy, requestClassSize );
        DLD_DBG_MAKE_POINTER_INVALID( requestCopy );
        
        gDldDbgData.gScsiCallbackOffset = gCallbackOffset;
                   
    }// if( (vm_offset_t)(-1) == gCallbackOffset )
                   
    
    task = new DldSCSITask();
    if( !task->init() ){// super::init()
        
        task->release();
        return NULL;
    }
    
    DldInitNotificationEvent( &task->shadowCompletionEvent );
    
    //
    // get a data buffer descriptor
    //
    task->dataBuffer = DldSCSITaskGetDataBuffer( request );
    if( task->dataBuffer )
        task->dataBuffer->retain();
    
    task->request = request;
    request->retain();// reference the request object
    
    task->dldService = dldService;
    dldService->retain();// reference the DVD drive object
    
    //
    // insert in the list
    //
    DldSCSITask::LockExclusive();
    {// start of the lock
        CIRCLEQ_INSERT_TAIL_WITH_TYPE( &DldSCSITask::gSCSITaskListHead, task, listEntry, DldSCSITask );
    }// end of the lock
    DldSCSITask::UnlockExclusive();
    
    return task;
}

//--------------------------------------------------------------------

void DldSCSITask::free()
{
    assert( preemption_enabled() );
    
    if( this->completionHooked )
        this->RestoreOriginalTaskCompletionCallback();
            
    if( this->listEntry.cqe_next ){
        
        DldSCSITask::LockExclusive();
        {// start of the lock
            CIRCLEQ_REMOVE( &DldSCSITask::gSCSITaskListHead, this, listEntry);
        }// end of the lock
        DldSCSITask::UnlockExclusive();
    }
    
    if( this->dataBuffer ){
        
        while( 0x0 != this->bufferPreparedCount ){
            
            this->dataBuffer->complete();
            this->bufferPreparedCount -= 0x1;
            
        }// end while
        
        this->dataBuffer->release();
        
    }// end if( this->dataBuffer )
    
    if( this->request )
        this->request->release();
    
    if( this->dldService )
        this->dldService->release();
    
    super::free();
}

//--------------------------------------------------------------------

void DldSCSITask::waitForShadowCompletion()
{
    assert( preemption_enabled() );
    assert( 0x0 != this->shadowOperationID );// must be shadowed
    
    if( 0x0 == this->shadowOperationID )
        return;
    
    //
    // wait for shadow completion
    //
    DldWaitForNotificationEvent( &this->shadowCompletionEvent );
}

//--------------------------------------------------------------------

//
// returns a refernced task for the request, the task is
// returned only if it has been found in the list
//
DldSCSITask* DldSCSITask::GetReferencedSCSITask( __in SCSITaskIdentifier   request )
{
    DldSCSITask*   foundTask = NULL;
    
    DldSCSITask::LockShared();
    {// start of the lock
        
        DldSCSITask* entry;
        
        CIRCLEQ_FOREACH( entry, &DldSCSITask::gSCSITaskListHead, listEntry ){
            // start CIRCLEQ_FOREACH
            
            if( entry->request != request )
                continue;
            
            entry->retain();
            foundTask = entry;
            break;
            
        }// end CIRCLEQ_FOREACH
    }// end of the lock
    DldSCSITask::UnlockShared();
    
    return foundTask;
}

//--------------------------------------------------------------------

void DldSCSITask::LockShared()
{
    assert( rwLock );
    assert( preemption_enabled() );
    
    IORWLockRead( rwLock );
    
    assert( NULL == exclusiveThread );
    
}

void DldSCSITask::UnlockShared()
{
    assert( rwLock );
    assert( preemption_enabled() );
    assert( NULL == exclusiveThread );
    
    IORWLockUnlock( rwLock );
}

//--------------------------------------------------------------------

void DldSCSITask::LockExclusive()
{
    assert( rwLock );
    assert( preemption_enabled() );
    
#if defined( DBG )
    assert( current_thread() != exclusiveThread );
#endif//DBG
    
    IORWLockWrite( rwLock );
    
#if defined( DBG )
    exclusiveThread = current_thread();
#endif//DBG
    
}

void DldSCSITask::UnlockExclusive()
{
    assert( rwLock );
    assert( preemption_enabled() );
    
#if defined( DBG )
    assert( current_thread() == exclusiveThread );
    exclusiveThread = NULL;
#endif//DBG
    
    IORWLockUnlock( rwLock );
}

//--------------------------------------------------------------------

IOReturn DldSCSITask::prepareDataBuffer()
{
    IOReturn  retval;
    
    if( !this->dataBuffer )
        return kIOReturnSuccess;
    
    retval = this->dataBuffer->prepare();
    if( kIOReturnSuccess == retval ){
        
        OSIncrementAtomic( &this->bufferPreparedCount );
    }
    
    return retval;
}

//--------------------------------------------------------------------

//
// hooks the completion callback, only one hook is allowed per DldSCSITask
//
bool DldSCSITask::SetTaskCompletionCallbackHook( __in SCSITaskCompletion completionHook )
{
    assert( (vm_offset_t)(-1) != gCallbackOffset );
    
    //
    // a chain of hooks is not supported
    //
    assert( NULL == this->originalCompletionCallback );
    
    this->originalCompletionCallback = this->getCompletionRoutine();
    
    this->completionHooked = DldSCSITaskSetTaskCompletionCallback( this->request, completionHook );
    
    return this->completionHooked;
}

//--------------------------------------------------------------------

//
// restores the original callback value
//
bool DldSCSITask::RestoreOriginalTaskCompletionCallback()
{
    assert( (vm_offset_t)(-1) != gCallbackOffset );
    
    if( NULL == this->originalCompletionCallback )
        return true;
    
    this->completionHooked = ( false != DldSCSITaskSetTaskCompletionCallback( this->request, this->originalCompletionCallback ) );
    
    return !this->completionHooked;
}

//--------------------------------------------------------------------

//
// calls the original completion if it exists,
// must be called only from a completion hook
//
void DldSCSITask::CallOriginalCompletion()
{
    
    if( NULL == this->originalCompletionCallback )
        return;
    
    //
    // restore the original callback
    //
    this->RestoreOriginalTaskCompletionCallback();
    
    //
    // call the callback
    //
    (this->originalCompletionCallback)( this->request );
}

//--------------------------------------------------------------------

//
// an example for a completion hook, not used
//
void DldSCSITask::CommonSCSITaskHook( __in SCSITaskIdentifier   request )
{
    assert( preemption_enabled() );
    
    DldSCSITask* referncedTask;
    
    referncedTask = DldSCSITask::GetReferencedSCSITask( request );
    assert( referncedTask );
    if( !referncedTask ){
        
        DBG_PRINT_ERROR(("DldSCSITask::GetReferencedSCSITask( 0x%p ) returned NULL, this is an error!", request));
        return;
    }
    
    //
    // call an original completion
    //
    referncedTask->CallOriginalCompletion();
    
    //
    // the first refernce came from GetReferencedSCSITask()
    //
    referncedTask->release();
    
    //
    // we don't need the task anymore
    //
    referncedTask->release();
}

//--------------------------------------------------------------------

bool DldSCSITask::GetCommandDescriptorBlock( __inout SCSICommandDescriptorBlock * cdbData )
{
    return DldSCSITaskGetCommandDescriptorBlock( this->request, cdbData );
}

//--------------------------------------------------------------------

bool DldSCSITask::SetServiceResponse( __in SCSIServiceResponse serviceResponse )
{
    return DldSCSITaskSetServiceResponse( this->request, serviceResponse );
}

//--------------------------------------------------------------------

bool DldSCSITask::SetTaskStatus ( __in SCSITaskStatus newTaskStatus )
{
    return DldSCSITaskSetTaskStatus( this->request, newTaskStatus );
}

//--------------------------------------------------------------------

void DldSCSITask::TaskCompletedNotification()
{
    DldSCSITaskCompletedNotification( this->request );
}

//--------------------------------------------------------------------

UInt8 DldSCSITask::GetCommandDescriptorBlockSize()
{
    return DldSCSITaskGetCommandDescriptorBlockSize( this->request );
}

//--------------------------------------------------------------------

SCSIServiceResponse DldSCSITask::GetServiceResponse()
{
    return DldSCSITaskGetServiceResponse( this->request );
}

//--------------------------------------------------------------------

SCSITaskStatus DldSCSITask::GetTaskStatus()
{
    return DldSCSITaskGetTaskStatus( this->request );
}

//--------------------------------------------------------------------

UInt64	DldSCSITask::GetRealizedDataTransferCount()
{
    return DldSCSITaskGetRealizedDataTransferCount( this->request );
}

//--------------------------------------------------------------------

IOMemoryDescriptor* DldSCSITask::GetDataBuffer()
{
    return DldSCSITaskGetDataBuffer( this->request );
}

//--------------------------------------------------------------------

UInt64 DldSCSITask::GetRequestedDataTransferCount()
{
    return DldSCSITaskGetRequestedDataTransferCount( this->request );
}

//--------------------------------------------------------------------

UInt64 DldSCSITask::GetDataBufferOffset()
{
    return DldSCSITaskGetDataBufferOffset( this->request );
}

//--------------------------------------------------------------------

//
// completes the request, can't be called inside callback!
//
void DldSCSITask::CompleteAccessDenied()
{
    DldSCSITaskCompleteAccessDenied( this->request );
}

//--------------------------------------------------------------------

dld_classic_rights_t DldSCSITask::GetCdbRequestedAccess()
{
    return DldSCSITaskGetCdbRequestedAccess( this->request );
}

//--------------------------------------------------------------------