/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * HISTORY
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:48  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:26:09  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.1.11.2  1995/01/06  19:11:11  devrcs
 * 	mk6 CR668 - 1.3b26 merge
 * 	Add padding for alpha, make n_other unsigned,
 * 	fix erroneous def of N_FN.
 * 	[1994/10/14  03:40:03  dwm]
 *
 * Revision 1.1.11.1  1994/09/23  01:23:37  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:11:49  ezf]
 * 
 * Revision 1.1.4.3  1993/07/27  18:28:42  elliston
 * 	Add ANSI prototypes.  CR #9523.
 * 	[1993/07/27  18:13:44  elliston]
 * 
 * Revision 1.1.4.2  1993/06/02  23:13:34  jeffc
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  20:58:08  jeffc]
 * 
 * Revision 1.1  1992/09/30  02:24:29  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.4  91/05/14  15:38:20  mrt
 * 	Correcting copyright
 * 
 * Revision 2.3  91/02/05  17:07:42  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:20:26  mrt]
 * 
 * 11-Aug-88  David Golub (dbg) at Carnegie-Mellon University
 *	Added n_un, n_strx definitions for kernel debugger (from
 *	a.out.h).
 *
 */
/* CMU_ENDHIST */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *  nlist.h - symbol table entry  structure for an a.out file
 *  derived from FSF's a.out.gnu.h
 *
 */

#ifndef _DDB_NLIST_H_
#define _DDB_NLIST_H_

struct	nlist {
	union n_un {
	    char	*n_name;	/* symbol name */
	    long	n_strx;		/* index into file string table */
	} n_un;
	unsigned char n_type;	/* type flag, i.e. N_TEXT etc; see below */
	unsigned char n_other;	/* unused */
	short	n_desc;		/* see <stab.h> */
#if	defined(__alpha)
	int	n_pad;		/* alignment, used to carry framesize info */
#endif
	vm_offset_t n_value;	/* value of this symbol (or sdb offset) */
};


/*
 * Symbols with a index into the string table of zero (n_un.n_strx == 0) are
 * defined to have a null, "", name.  Therefore all string indexes to non null
 * names must not have a zero string index.  This is bit historical information
 * that has never been well documented.
 */

/*
 * The n_type field really contains three fields:
 *	unsigned char N_STAB:3,
 *		      N_PEXT:1,
 *		      N_TYPE:3,
 *		      N_EXT:1;
 * which are used via the following masks.
 */
#define	N_STAB	0xe0  /* if any of these bits set, a symbolic debugging entry */
#define	N_PEXT	0x10  /* private external symbol bit */
#define	N_TYPE	0x0e  /* mask for the type bits */
#define	N_EXT	0x01  /* external symbol bit, set for external symbols */

/*
 * Only symbolic debugging entries have some of the N_STAB bits set and if any
 * of these bits are set then it is a symbolic debugging entry (a stab).  In
 * which case then the values of the n_type field (the entire field) are given
 * in <mach-o/stab.h>
 */

/*
 * Values for N_TYPE bits of the n_type field.
 */
#define	N_UNDF	0x0		/* undefined, n_sect == NO_SECT */
#define	N_ABS	0x2		/* absolute, n_sect == NO_SECT */
#define	N_SECT	0xe		/* defined in section number n_sect */
#define	N_PBUD	0xc		/* prebound undefined (defined in a dylib) */
#define N_INDR	0xa		/* indirect */

/* 
 * If the type is N_INDR then the symbol is defined to be the same as another
 * symbol.  In this case the n_value field is an index into the string table
 * of the other symbol's name.  When the other symbol is defined then they both
 * take on the defined type and value.
 */

/*
 * If the type is N_SECT then the n_sect field contains an ordinal of the
 * section the symbol is defined in.  The sections are numbered from 1 and 
 * refer to sections in order they appear in the load commands for the file
 * they are in.  This means the same ordinal may very well refer to different
 * sections in different files.
 *
 * The n_value field for all symbol table entries (including N_STAB's) gets
 * updated by the link editor based on the value of it's n_sect field and where
 * the section n_sect references gets relocated.  If the value of the n_sect 
 * field is NO_SECT then it's n_value field is not changed by the link editor.
 */
#define	NO_SECT		0	/* symbol is not in any section */
#define MAX_SECT	255	/* 1 thru 255 inclusive */

#endif	/* !_DDB_NLIST_H_ */
