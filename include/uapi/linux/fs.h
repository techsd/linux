/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_FS_H
#define _UAPI_LINUX_FS_H

/*
 * This file has definitions for some important file table structures
 * and constants and structures used by various generic file system
 * ioctl's.  Please do not make any changes in this file before
 * sending patches for review to linux-fsdevel@vger.kernel.org and
 * linux-api@vger.kernel.org.
 */

#include <linux/limits.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#ifndef __KERNEL__
#include <linux/fscrypt.h>
#endif

/* Use of MS_* flags within the kernel is restricted to core mount(2) code. */
#if !defined(__KERNEL__)
#include <linux/mount.h>
#endif

/*
 * It's silly to have NR_OPEN bigger than NR_FILE, but you can change
 * the file limit at runtime and only root can increase the per-process
 * nr_file rlimit, so it's safe to set up a ridiculously high absolute
 * upper limit on files-per-process.
 *
 * Some programs (notably those using select()) may have to be 
 * recompiled to take full advantage of the new limits..  
 */

/* Fixed constants first: */
#undef NR_OPEN
#define INR_OPEN_CUR 1024	/* Initial setting for nfile rlimits */
#define INR_OPEN_MAX 4096	/* Hard limit for nfile rlimits */

#define BLOCK_SIZE_BITS 10
#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)

/* flags for integrity meta */
#define IO_INTEGRITY_CHK_GUARD		(1U << 0) /* enforce guard check */
#define IO_INTEGRITY_CHK_REFTAG		(1U << 1) /* enforce ref check */
#define IO_INTEGRITY_CHK_APPTAG		(1U << 2) /* enforce app check */

#define IO_INTEGRITY_VALID_FLAGS (IO_INTEGRITY_CHK_GUARD | \
				  IO_INTEGRITY_CHK_REFTAG | \
				  IO_INTEGRITY_CHK_APPTAG)

#define SEEK_SET	0	/* seek relative to beginning of file */
#define SEEK_CUR	1	/* seek relative to current file position */
#define SEEK_END	2	/* seek relative to end of file */
#define SEEK_DATA	3	/* seek to the next data */
#define SEEK_HOLE	4	/* seek to the next hole */
#define SEEK_MAX	SEEK_HOLE

#define RENAME_NOREPLACE	(1 << 0)	/* Don't overwrite target */
#define RENAME_EXCHANGE		(1 << 1)	/* Exchange source and dest */
#define RENAME_WHITEOUT		(1 << 2)	/* Whiteout source */

/*
 * The root inode of procfs is guaranteed to always have the same inode number.
 * For programs that make heavy use of procfs, verifying that the root is a
 * real procfs root and using openat2(RESOLVE_{NO_{XDEV,MAGICLINKS},BENEATH})
 * will allow you to make sure you are never tricked into operating on the
 * wrong procfs file.
 */
enum procfs_ino {
	PROCFS_ROOT_INO = 1,
};

struct file_clone_range {
	__s64 src_fd;
	__u64 src_offset;
	__u64 src_length;
	__u64 dest_offset;
};

struct fstrim_range {
	__u64 start;
	__u64 len;
	__u64 minlen;
};

/*
 * We include a length field because some filesystems (vfat) have an identifier
 * that we do want to expose as a UUID, but doesn't have the standard length.
 *
 * We use a fixed size buffer beacuse this interface will, by fiat, never
 * support "UUIDs" longer than 16 bytes; we don't want to force all downstream
 * users to have to deal with that.
 */
struct fsuuid2 {
	__u8	len;
	__u8	uuid[16];
};

struct fs_sysfs_path {
	__u8			len;
	__u8			name[128];
};

/* Protection info capability flags */
#define	LBMD_PI_CAP_INTEGRITY		(1 << 0)
#define	LBMD_PI_CAP_REFTAG		(1 << 1)

/* Checksum types for Protection Information */
#define LBMD_PI_CSUM_NONE		0
#define LBMD_PI_CSUM_IP			1
#define LBMD_PI_CSUM_CRC16_T10DIF	2
#define LBMD_PI_CSUM_CRC64_NVME		4

/* sizeof first published struct */
#define LBMD_SIZE_VER0			16

/*
 * Logical block metadata capability descriptor
 * If the device does not support metadata, all the fields will be zero.
 * Applications must check lbmd_flags to determine whether metadata is
 * supported or not.
 */
struct logical_block_metadata_cap {
	/* Bitmask of logical block metadata capability flags */
	__u32	lbmd_flags;
	/*
	 * The amount of data described by each unit of logical block
	 * metadata
	 */
	__u16	lbmd_interval;
	/*
	 * Size in bytes of the logical block metadata associated with each
	 * interval
	 */
	__u8	lbmd_size;
	/*
	 * Size in bytes of the opaque block tag associated with each
	 * interval
	 */
	__u8	lbmd_opaque_size;
	/*
	 * Offset in bytes of the opaque block tag within the logical block
	 * metadata
	 */
	__u8	lbmd_opaque_offset;
	/* Size in bytes of the T10 PI tuple associated with each interval */
	__u8	lbmd_pi_size;
	/* Offset in bytes of T10 PI tuple within the logical block metadata */
	__u8	lbmd_pi_offset;
	/* T10 PI guard tag type */
	__u8	lbmd_guard_tag_type;
	/* Size in bytes of the T10 PI application tag */
	__u8	lbmd_app_tag_size;
	/* Size in bytes of the T10 PI reference tag */
	__u8	lbmd_ref_tag_size;
	/* Size in bytes of the T10 PI storage tag */
	__u8	lbmd_storage_tag_size;
	__u8	pad;
};

/* extent-same (dedupe) ioctls; these MUST match the btrfs ioctl definitions */
#define FILE_DEDUPE_RANGE_SAME		0
#define FILE_DEDUPE_RANGE_DIFFERS	1

/* from struct btrfs_ioctl_file_extent_same_info */
struct file_dedupe_range_info {
	__s64 dest_fd;		/* in - destination file */
	__u64 dest_offset;	/* in - start of extent in destination */
	__u64 bytes_deduped;	/* out - total # of bytes we were able
				 * to dedupe from this file. */
	/* status of this dedupe operation:
	 * < 0 for error
	 * == FILE_DEDUPE_RANGE_SAME if dedupe succeeds
	 * == FILE_DEDUPE_RANGE_DIFFERS if data differs
	 */
	__s32 status;		/* out - see above description */
	__u32 reserved;		/* must be zero */
};

/* from struct btrfs_ioctl_file_extent_same_args */
struct file_dedupe_range {
	__u64 src_offset;	/* in - start of extent in source */
	__u64 src_length;	/* in - length of extent */
	__u16 dest_count;	/* in - total elements in info array */
	__u16 reserved1;	/* must be zero */
	__u32 reserved2;	/* must be zero */
	struct file_dedupe_range_info info[];
};

/* And dynamically-tunable limits and defaults: */
struct files_stat_struct {
	unsigned long nr_files;		/* read only */
	unsigned long nr_free_files;	/* read only */
	unsigned long max_files;		/* tunable */
};

struct inodes_stat_t {
	long nr_inodes;
	long nr_unused;
	long dummy[5];		/* padding for sysctl ABI compatibility */
};


#define NR_FILE  8192	/* this can well be larger on a larger system */

/*
 * Structure for FS_IOC_FSGETXATTR[A] and FS_IOC_FSSETXATTR.
 */
struct fsxattr {
	__u32		fsx_xflags;	/* xflags field value (get/set) */
	__u32		fsx_extsize;	/* extsize field value (get/set)*/
	__u32		fsx_nextents;	/* nextents field value (get)	*/
	__u32		fsx_projid;	/* project identifier (get/set) */
	__u32		fsx_cowextsize;	/* CoW extsize field value (get/set)*/
	unsigned char	fsx_pad[8];
};

/*
 * Variable size structure for file_[sg]et_attr().
 *
 * Note. This is alternative to the structure 'struct file_kattr'/'struct fsxattr'.
 * As this structure is passed to/from userspace with its size, this can
 * be versioned based on the size.
 */
struct file_attr {
	__u64 fa_xflags;	/* xflags field value (get/set) */
	__u32 fa_extsize;	/* extsize field value (get/set)*/
	__u32 fa_nextents;	/* nextents field value (get)   */
	__u32 fa_projid;	/* project identifier (get/set) */
	__u32 fa_cowextsize;	/* CoW extsize field value (get/set) */
};

#define FILE_ATTR_SIZE_VER0 24
#define FILE_ATTR_SIZE_LATEST FILE_ATTR_SIZE_VER0

/*
 * Flags for the fsx_xflags field
 */
#define FS_XFLAG_REALTIME	0x00000001	/* data in realtime volume */
#define FS_XFLAG_PREALLOC	0x00000002	/* preallocated file extents */
#define FS_XFLAG_IMMUTABLE	0x00000008	/* file cannot be modified */
#define FS_XFLAG_APPEND		0x00000010	/* all writes append */
#define FS_XFLAG_SYNC		0x00000020	/* all writes synchronous */
#define FS_XFLAG_NOATIME	0x00000040	/* do not update access time */
#define FS_XFLAG_NODUMP		0x00000080	/* do not include in backups */
#define FS_XFLAG_RTINHERIT	0x00000100	/* create with rt bit set */
#define FS_XFLAG_PROJINHERIT	0x00000200	/* create with parents projid */
#define FS_XFLAG_NOSYMLINKS	0x00000400	/* disallow symlink creation */
#define FS_XFLAG_EXTSIZE	0x00000800	/* extent size allocator hint */
#define FS_XFLAG_EXTSZINHERIT	0x00001000	/* inherit inode extent size */
#define FS_XFLAG_NODEFRAG	0x00002000	/* do not defragment */
#define FS_XFLAG_FILESTREAM	0x00004000	/* use filestream allocator */
#define FS_XFLAG_DAX		0x00008000	/* use DAX for IO */
#define FS_XFLAG_COWEXTSIZE	0x00010000	/* CoW extent size allocator hint */
#define FS_XFLAG_HASATTR	0x80000000	/* no DIFLAG for this	*/

/* the read-only stuff doesn't really belong here, but any other place is
   probably as bad and I don't want to create yet another include file. */

#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size /512 (long *arg) */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#define BLKFRASET  _IO(0x12,100)/* set filesystem (mm/filemap.c) read-ahead */
#define BLKFRAGET  _IO(0x12,101)/* get filesystem (mm/filemap.c) read-ahead */
#define BLKSECTSET _IO(0x12,102)/* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET _IO(0x12,103)/* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#if 0
#define BLKPG      _IO(0x12,105)/* See blkpg.h */

/* Some people are morons.  Do not use sizeof! */

#define BLKELVGET  _IOR(0x12,106,size_t)/* elevator get */
#define BLKELVSET  _IOW(0x12,107,size_t)/* elevator set */
/* This was here just to show that the number is taken -
   probably all these _IO(0x12,*) ioctls should be moved to blkpg.h. */
#endif
/* A jump here: 108-111 have been used for various private purposes. */
#define BLKBSZGET  _IOR(0x12,112,size_t)
#define BLKBSZSET  _IOW(0x12,113,size_t)
#define BLKGETSIZE64 _IOR(0x12,114,size_t)	/* return device size in bytes (u64 *arg) */
#define BLKTRACESETUP _IOWR(0x12,115,struct blk_user_trace_setup)
#define BLKTRACESTART _IO(0x12,116)
#define BLKTRACESTOP _IO(0x12,117)
#define BLKTRACETEARDOWN _IO(0x12,118)
#define BLKDISCARD _IO(0x12,119)
#define BLKIOMIN _IO(0x12,120)
#define BLKIOOPT _IO(0x12,121)
#define BLKALIGNOFF _IO(0x12,122)
#define BLKPBSZGET _IO(0x12,123)
#define BLKDISCARDZEROES _IO(0x12,124)
#define BLKSECDISCARD _IO(0x12,125)
#define BLKROTATIONAL _IO(0x12,126)
#define BLKZEROOUT _IO(0x12,127)
#define BLKGETDISKSEQ _IOR(0x12,128,__u64)
/* 130-136 are used by zoned block device ioctls (uapi/linux/blkzoned.h) */
/* 137-141 are used by blk-crypto ioctls (uapi/linux/blk-crypto.h) */

#define BMAP_IOCTL 1		/* obsolete - kept for compatibility */
#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */
#define FIFREEZE	_IOWR('X', 119, int)	/* Freeze */
#define FITHAW		_IOWR('X', 120, int)	/* Thaw */
#define FITRIM		_IOWR('X', 121, struct fstrim_range)	/* Trim */
#define FICLONE		_IOW(0x94, 9, int)
#define FICLONERANGE	_IOW(0x94, 13, struct file_clone_range)
#define FIDEDUPERANGE	_IOWR(0x94, 54, struct file_dedupe_range)

#define FSLABEL_MAX 256	/* Max chars for the interface; each fs may differ */

#define	FS_IOC_GETFLAGS			_IOR('f', 1, long)
#define	FS_IOC_SETFLAGS			_IOW('f', 2, long)
#define	FS_IOC_GETVERSION		_IOR('v', 1, long)
#define	FS_IOC_SETVERSION		_IOW('v', 2, long)
#define FS_IOC_FIEMAP			_IOWR('f', 11, struct fiemap)
#define FS_IOC32_GETFLAGS		_IOR('f', 1, int)
#define FS_IOC32_SETFLAGS		_IOW('f', 2, int)
#define FS_IOC32_GETVERSION		_IOR('v', 1, int)
#define FS_IOC32_SETVERSION		_IOW('v', 2, int)
#define FS_IOC_FSGETXATTR		_IOR('X', 31, struct fsxattr)
#define FS_IOC_FSSETXATTR		_IOW('X', 32, struct fsxattr)
#define FS_IOC_GETFSLABEL		_IOR(0x94, 49, char[FSLABEL_MAX])
#define FS_IOC_SETFSLABEL		_IOW(0x94, 50, char[FSLABEL_MAX])
/* Returns the external filesystem UUID, the same one blkid returns */
#define FS_IOC_GETFSUUID		_IOR(0x15, 0, struct fsuuid2)
/*
 * Returns the path component under /sys/fs/ that refers to this filesystem;
 * also /sys/kernel/debug/ for filesystems with debugfs exports
 */
#define FS_IOC_GETFSSYSFSPATH		_IOR(0x15, 1, struct fs_sysfs_path)
/* Get logical block metadata capability details */
#define FS_IOC_GETLBMD_CAP		_IOWR(0x15, 2, struct logical_block_metadata_cap)

/*
 * Inode flags (FS_IOC_GETFLAGS / FS_IOC_SETFLAGS)
 *
 * Note: for historical reasons, these flags were originally used and
 * defined for use by ext2/ext3, and then other file systems started
 * using these flags so they wouldn't need to write their own version
 * of chattr/lsattr (which was shipped as part of e2fsprogs).  You
 * should think twice before trying to use these flags in new
 * contexts, or trying to assign these flags, since they are used both
 * as the UAPI and the on-disk encoding for ext2/3/4.  Also, we are
 * almost out of 32-bit flags.  :-)
 *
 * We have recently hoisted FS_IOC_FSGETXATTR / FS_IOC_FSSETXATTR from
 * XFS to the generic FS level interface.  This uses a structure that
 * has padding and hence has more room to grow, so it may be more
 * appropriate for many new use cases.
 *
 * Please do not change these flags or interfaces before checking with
 * linux-fsdevel@vger.kernel.org and linux-api@vger.kernel.org.
 */
#define	FS_SECRM_FL			0x00000001 /* Secure deletion */
#define	FS_UNRM_FL			0x00000002 /* Undelete */
#define	FS_COMPR_FL			0x00000004 /* Compress file */
#define FS_SYNC_FL			0x00000008 /* Synchronous updates */
#define FS_IMMUTABLE_FL			0x00000010 /* Immutable file */
#define FS_APPEND_FL			0x00000020 /* writes to file may only append */
#define FS_NODUMP_FL			0x00000040 /* do not dump file */
#define FS_NOATIME_FL			0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define FS_DIRTY_FL			0x00000100
#define FS_COMPRBLK_FL			0x00000200 /* One or more compressed clusters */
#define FS_NOCOMP_FL			0x00000400 /* Don't compress */
/* End compression flags --- maybe not all used */
#define FS_ENCRYPT_FL			0x00000800 /* Encrypted file */
#define FS_BTREE_FL			0x00001000 /* btree format dir */
#define FS_INDEX_FL			0x00001000 /* hash-indexed directory */
#define FS_IMAGIC_FL			0x00002000 /* AFS directory */
#define FS_JOURNAL_DATA_FL		0x00004000 /* Reserved for ext3 */
#define FS_NOTAIL_FL			0x00008000 /* file tail should not be merged */
#define FS_DIRSYNC_FL			0x00010000 /* dirsync behaviour (directories only) */
#define FS_TOPDIR_FL			0x00020000 /* Top of directory hierarchies*/
#define FS_HUGE_FILE_FL			0x00040000 /* Reserved for ext4 */
#define FS_EXTENT_FL			0x00080000 /* Extents */
#define FS_VERITY_FL			0x00100000 /* Verity protected inode */
#define FS_EA_INODE_FL			0x00200000 /* Inode used for large EA */
#define FS_EOFBLOCKS_FL			0x00400000 /* Reserved for ext4 */
#define FS_NOCOW_FL			0x00800000 /* Do not cow file */
#define FS_DAX_FL			0x02000000 /* Inode is DAX */
#define FS_INLINE_DATA_FL		0x10000000 /* Reserved for ext4 */
#define FS_PROJINHERIT_FL		0x20000000 /* Create with parents projid */
#define FS_CASEFOLD_FL			0x40000000 /* Folder is case insensitive */
#define FS_RESERVED_FL			0x80000000 /* reserved for ext2 lib */

#define FS_FL_USER_VISIBLE		0x0003DFFF /* User visible flags */
#define FS_FL_USER_MODIFIABLE		0x000380FF /* User modifiable flags */


#define SYNC_FILE_RANGE_WAIT_BEFORE	1
#define SYNC_FILE_RANGE_WRITE		2
#define SYNC_FILE_RANGE_WAIT_AFTER	4
#define SYNC_FILE_RANGE_WRITE_AND_WAIT	(SYNC_FILE_RANGE_WRITE | \
					 SYNC_FILE_RANGE_WAIT_BEFORE | \
					 SYNC_FILE_RANGE_WAIT_AFTER)

/*
 * Flags for preadv2/pwritev2:
 */

typedef int __bitwise __kernel_rwf_t;

/* high priority request, poll if possible */
#define RWF_HIPRI	((__force __kernel_rwf_t)0x00000001)

/* per-IO O_DSYNC */
#define RWF_DSYNC	((__force __kernel_rwf_t)0x00000002)

/* per-IO O_SYNC */
#define RWF_SYNC	((__force __kernel_rwf_t)0x00000004)

/* per-IO, return -EAGAIN if operation would block */
#define RWF_NOWAIT	((__force __kernel_rwf_t)0x00000008)

/* per-IO O_APPEND */
#define RWF_APPEND	((__force __kernel_rwf_t)0x00000010)

/* per-IO negation of O_APPEND */
#define RWF_NOAPPEND	((__force __kernel_rwf_t)0x00000020)

/* Atomic Write */
#define RWF_ATOMIC	((__force __kernel_rwf_t)0x00000040)

/* buffered IO that drops the cache after reading or writing data */
#define RWF_DONTCACHE	((__force __kernel_rwf_t)0x00000080)

/* mask of flags supported by the kernel */
#define RWF_SUPPORTED	(RWF_HIPRI | RWF_DSYNC | RWF_SYNC | RWF_NOWAIT |\
			 RWF_APPEND | RWF_NOAPPEND | RWF_ATOMIC |\
			 RWF_DONTCACHE)

#define PROCFS_IOCTL_MAGIC 'f'

/* Pagemap ioctl */
#define PAGEMAP_SCAN	_IOWR(PROCFS_IOCTL_MAGIC, 16, struct pm_scan_arg)

/* Bitmasks provided in pm_scan_args masks and reported in page_region.categories. */
#define PAGE_IS_WPALLOWED	(1 << 0)
#define PAGE_IS_WRITTEN		(1 << 1)
#define PAGE_IS_FILE		(1 << 2)
#define PAGE_IS_PRESENT		(1 << 3)
#define PAGE_IS_SWAPPED		(1 << 4)
#define PAGE_IS_PFNZERO		(1 << 5)
#define PAGE_IS_HUGE		(1 << 6)
#define PAGE_IS_SOFT_DIRTY	(1 << 7)
#define PAGE_IS_GUARD		(1 << 8)

/*
 * struct page_region - Page region with flags
 * @start:	Start of the region
 * @end:	End of the region (exclusive)
 * @categories:	PAGE_IS_* category bitmask for the region
 */
struct page_region {
	__u64 start;
	__u64 end;
	__u64 categories;
};

/* Flags for PAGEMAP_SCAN ioctl */
#define PM_SCAN_WP_MATCHING	(1 << 0)	/* Write protect the pages matched. */
#define PM_SCAN_CHECK_WPASYNC	(1 << 1)	/* Abort the scan when a non-WP-enabled page is found. */

/*
 * struct pm_scan_arg - Pagemap ioctl argument
 * @size:		Size of the structure
 * @flags:		Flags for the IOCTL
 * @start:		Starting address of the region
 * @end:		Ending address of the region
 * @walk_end		Address where the scan stopped (written by kernel).
 *			walk_end == end (address tags cleared) informs that the scan completed on entire range.
 * @vec:		Address of page_region struct array for output
 * @vec_len:		Length of the page_region struct array
 * @max_pages:		Optional limit for number of returned pages (0 = disabled)
 * @category_inverted:	PAGE_IS_* categories which values match if 0 instead of 1
 * @category_mask:	Skip pages for which any category doesn't match
 * @category_anyof_mask: Skip pages for which no category matches
 * @return_mask:	PAGE_IS_* categories that are to be reported in `page_region`s returned
 */
struct pm_scan_arg {
	__u64 size;
	__u64 flags;
	__u64 start;
	__u64 end;
	__u64 walk_end;
	__u64 vec;
	__u64 vec_len;
	__u64 max_pages;
	__u64 category_inverted;
	__u64 category_mask;
	__u64 category_anyof_mask;
	__u64 return_mask;
};

/* /proc/<pid>/maps ioctl */
#define PROCMAP_QUERY	_IOWR(PROCFS_IOCTL_MAGIC, 17, struct procmap_query)

enum procmap_query_flags {
	/*
	 * VMA permission flags.
	 *
	 * Can be used as part of procmap_query.query_flags field to look up
	 * only VMAs satisfying specified subset of permissions. E.g., specifying
	 * PROCMAP_QUERY_VMA_READABLE only will return both readable and read/write VMAs,
	 * while having PROCMAP_QUERY_VMA_READABLE | PROCMAP_QUERY_VMA_WRITABLE will only
	 * return read/write VMAs, though both executable/non-executable and
	 * private/shared will be ignored.
	 *
	 * PROCMAP_QUERY_VMA_* flags are also returned in procmap_query.vma_flags
	 * field to specify actual VMA permissions.
	 */
	PROCMAP_QUERY_VMA_READABLE		= 0x01,
	PROCMAP_QUERY_VMA_WRITABLE		= 0x02,
	PROCMAP_QUERY_VMA_EXECUTABLE		= 0x04,
	PROCMAP_QUERY_VMA_SHARED		= 0x08,
	/*
	 * Query modifier flags.
	 *
	 * By default VMA that covers provided address is returned, or -ENOENT
	 * is returned. With PROCMAP_QUERY_COVERING_OR_NEXT_VMA flag set, closest
	 * VMA with vma_start > addr will be returned if no covering VMA is
	 * found.
	 *
	 * PROCMAP_QUERY_FILE_BACKED_VMA instructs query to consider only VMAs that
	 * have file backing. Can be combined with PROCMAP_QUERY_COVERING_OR_NEXT_VMA
	 * to iterate all VMAs with file backing.
	 */
	PROCMAP_QUERY_COVERING_OR_NEXT_VMA	= 0x10,
	PROCMAP_QUERY_FILE_BACKED_VMA		= 0x20,
};

/*
 * Input/output argument structured passed into ioctl() call. It can be used
 * to query a set of VMAs (Virtual Memory Areas) of a process.
 *
 * Each field can be one of three kinds, marked in a short comment to the
 * right of the field:
 *   - "in", input argument, user has to provide this value, kernel doesn't modify it;
 *   - "out", output argument, kernel sets this field with VMA data;
 *   - "in/out", input and output argument; user provides initial value (used
 *     to specify maximum allowable buffer size), and kernel sets it to actual
 *     amount of data written (or zero, if there is no data).
 *
 * If matching VMA is found (according to criterias specified by
 * query_addr/query_flags, all the out fields are filled out, and ioctl()
 * returns 0. If there is no matching VMA, -ENOENT will be returned.
 * In case of any other error, negative error code other than -ENOENT is
 * returned.
 *
 * Most of the data is similar to the one returned as text in /proc/<pid>/maps
 * file, but procmap_query provides more querying flexibility. There are no
 * consistency guarantees between subsequent ioctl() calls, but data returned
 * for matched VMA is self-consistent.
 */
struct procmap_query {
	/* Query struct size, for backwards/forward compatibility */
	__u64 size;
	/*
	 * Query flags, a combination of enum procmap_query_flags values.
	 * Defines query filtering and behavior, see enum procmap_query_flags.
	 *
	 * Input argument, provided by user. Kernel doesn't modify it.
	 */
	__u64 query_flags;		/* in */
	/*
	 * Query address. By default, VMA that covers this address will
	 * be looked up. PROCMAP_QUERY_* flags above modify this default
	 * behavior further.
	 *
	 * Input argument, provided by user. Kernel doesn't modify it.
	 */
	__u64 query_addr;		/* in */
	/* VMA starting (inclusive) and ending (exclusive) address, if VMA is found. */
	__u64 vma_start;		/* out */
	__u64 vma_end;			/* out */
	/* VMA permissions flags. A combination of PROCMAP_QUERY_VMA_* flags. */
	__u64 vma_flags;		/* out */
	/* VMA backing page size granularity. */
	__u64 vma_page_size;		/* out */
	/*
	 * VMA file offset. If VMA has file backing, this specifies offset
	 * within the file that VMA's start address corresponds to.
	 * Is set to zero if VMA has no backing file.
	 */
	__u64 vma_offset;		/* out */
	/* Backing file's inode number, or zero, if VMA has no backing file. */
	__u64 inode;			/* out */
	/* Backing file's device major/minor number, or zero, if VMA has no backing file. */
	__u32 dev_major;		/* out */
	__u32 dev_minor;		/* out */
	/*
	 * If set to non-zero value, signals the request to return VMA name
	 * (i.e., VMA's backing file's absolute path, with " (deleted)" suffix
	 * appended, if file was unlinked from FS) for matched VMA. VMA name
	 * can also be some special name (e.g., "[heap]", "[stack]") or could
	 * be even user-supplied with prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME).
	 *
	 * Kernel will set this field to zero, if VMA has no associated name.
	 * Otherwise kernel will return actual amount of bytes filled in
	 * user-supplied buffer (see vma_name_addr field below), including the
	 * terminating zero.
	 *
	 * If VMA name is longer that user-supplied maximum buffer size,
	 * -E2BIG error is returned.
	 *
	 * If this field is set to non-zero value, vma_name_addr should point
	 * to valid user space memory buffer of at least vma_name_size bytes.
	 * If set to zero, vma_name_addr should be set to zero as well
	 */
	__u32 vma_name_size;		/* in/out */
	/*
	 * If set to non-zero value, signals the request to extract and return
	 * VMA's backing file's build ID, if the backing file is an ELF file
	 * and it contains embedded build ID.
	 *
	 * Kernel will set this field to zero, if VMA has no backing file,
	 * backing file is not an ELF file, or ELF file has no build ID
	 * embedded.
	 *
	 * Build ID is a binary value (not a string). Kernel will set
	 * build_id_size field to exact number of bytes used for build ID.
	 * If build ID is requested and present, but needs more bytes than
	 * user-supplied maximum buffer size (see build_id_addr field below),
	 * -E2BIG error will be returned.
	 *
	 * If this field is set to non-zero value, build_id_addr should point
	 * to valid user space memory buffer of at least build_id_size bytes.
	 * If set to zero, build_id_addr should be set to zero as well
	 */
	__u32 build_id_size;		/* in/out */
	/*
	 * User-supplied address of a buffer of at least vma_name_size bytes
	 * for kernel to fill with matched VMA's name (see vma_name_size field
	 * description above for details).
	 *
	 * Should be set to zero if VMA name should not be returned.
	 */
	__u64 vma_name_addr;		/* in */
	/*
	 * User-supplied address of a buffer of at least build_id_size bytes
	 * for kernel to fill with matched VMA's ELF build ID, if available
	 * (see build_id_size field description above for details).
	 *
	 * Should be set to zero if build ID should not be returned.
	 */
	__u64 build_id_addr;		/* in */
};

#endif /* _UAPI_LINUX_FS_H */
