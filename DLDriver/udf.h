/*
  Author: Slava Imameyev
  Copyright (c) 2008 SmartLine, Inc.
  All Rights Reserved.
*/

/*
the file contains structure definitions for the UDF from the ECMA 167 specification
*/
#ifndef _DL_UDF_H
#define _DL_UDF_H

typedef unsigned char    ubyte;
typedef char    dstring;
typedef UCHAR   Uint8;
typedef USHORT  Uint16;
typedef SHORT   Int16;
typedef ULONG   Uint32;

#define UDF_AVDP_OFFSET_SECTOR    (256)
#define UDF_LVD_TAG               (0x6)
#define UDF_TD_TAG                (0x8)

#pragma pack(1)
typedef struct _udf_extent_ad{
    Uint32    ExtentLength;
    Uint32    ExtentLocation;
} udf_extent_ad, *pudf_extent_ad;

#pragma pack(1)
typedef struct _udf_tag { /* ECMA 167 3/7.2 */
    Uint16  TagIdentifier;
    Uint16  DescriptorVersion;
    Uint8   TagChecksum;
    ubyte   Reserved;
    Uint16  TagSerialNumber;
    Uint16  DescriptorCRC;
    Uint16  DescriptorCRCLength;
    Uint32  TagLocation;
} udf_tag, *pudf_tag;

#pragma pack(1)
typedef struct _udf_timestamp { /* ECMA 167 1/7.3 */
    Uint16  TypeAndTimezone;
    Int16   Year;
    Uint8   Month;
    Uint8   Day;
    Uint8   Hour;
    Uint8   Minute;
    Uint8   Second;
    Uint8   Centiseconds;
    Uint8   HundredsofMicroseconds;
    Uint8   Microseconds;
} udf_timestamp, *pudf_timestamp;

#pragma pack(1)
typedef struct _udf_AnchorVolumeDescriptorPointer { /* ECMA 167 3/10.2 */
    udf_tag         DescriptorTag;
    udf_extent_ad   MainVolumeDescriptorSequenceExtent;
    udf_extent_ad   ReserveVolumeDescriptorSequenceExtent;
    ubyte           Reserved[480];
} udf_AnchorVolumeDescriptorPointer, *pudf_AnchorVolumeDescriptorPointer;

#pragma pack(1)
typedef struct _udf_charspec { /* ECMA 167 1/7.2.1 */
    Uint8   CharacterSetType;
    ubyte   CharacterSetInfo[63];
} udf_charspec, *pudf_charspec;

#pragma pack(1)
typedef struct _udf_EntityID { /* ECMA 167 1/7.4 */
    Uint8 Flags;
    char Identifier[23];
    char IdentifierSuffix[8];
} udf_EntityID, *pudf_EntityID;

#pragma pack(1)
typedef struct _udf_LogicalVolumeDescriptor { /* ECMA 167 3/10.6 */
    udf_tag         DescriptorTag;
    Uint32          VolumeDescriptorSequenceNumber;
    udf_charspec    DescriptorCharacterSet;
    dstring         LogicalVolumeIdentifier[128];
    Uint32          LogicalBlockSize;
    udf_EntityID    DomainIdentifier;
    ubyte           LogicalVolumeContentsUse[16];
    Uint32          MapTableLength;
    Uint32          NumberofPartitionMaps;
    udf_EntityID    ImplementationIdentifier;
    ubyte           ImplementationUse[128];
    udf_extent_ad   IntegritySequenceExtent;
    ubyte           PartitionMaps[ 0x1 ];
} udf_LogicalVolumeDescriptor, *pudf_LogicalVolumeDescriptor;

//
// set the alignment to default
//
#pragma pack()

#endif //_DL_UDF_H