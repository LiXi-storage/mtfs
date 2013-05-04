/*
 * Copied from Lustre-2.x
 */

#ifndef __MTFS_LOG_H__
#define __MTFS_LOG_H__

#if defined (__linux__) && defined(__KERNEL__)

/** Identifier for a single log object */
struct mlog_logid {
        __u64                   mgl_oid;
        __u64                   mgl_ogr;
        __u32                   mgl_ogen;
} __attribute__((packed));

/** Log record header - stored in little endian order.
 * Each record must start with this struct, end with a mlog_rec_tail,
 * and be a multiple of 256 bits in size.
 */
struct mlog_rec_hdr {
        __u32                   mrh_len;
        __u32                   mrh_index;
        __u32                   mrh_type;
        __u32                   padding;
};

struct mlog_rec_tail {
        __u32 mrt_len;
        __u32 mrt_index;
};

struct mlog_gen {
        __u64 mnt_cnt;
        __u64 conn_cnt;
} __attribute__((packed));

/* On-disk header structure of each log object, stored in little endian order */
#define MLOG_CHUNK_SIZE         8192
#define MLOG_HEADER_SIZE        (96)
#define MLOG_BITMAP_BYTES       (MLOG_CHUNK_SIZE - MLOG_HEADER_SIZE)

#define MLOG_MIN_REC_SIZE       (24) /* round(mlog_rec_hdr + mlog_rec_tail) */

/* flags for the logs */
#define MLOG_F_ZAP_WHEN_EMPTY   0x1
#define MLOG_F_IS_CAT           0x2
#define MLOG_F_IS_PLAIN         0x4

struct mlog_log_hdr {
	struct mlog_rec_hdr     mlh_hdr;
	__u64                   mlh_timestamp;
	__u32                   mlh_count;
	__u32                   mlh_bitmap_offset;
	__u32                   mlh_size;
	__u32                   mlh_flags;
	__u32                   mlh_cat_idx;
	/* for a catalog the first plain slot is next to it */
	//struct obd_uuid         mlh_tgtuuid;
	__u32                   mlh_reserved[MLOG_HEADER_SIZE/sizeof(__u32) - 23];
	__u32                   mlh_bitmap[MLOG_BITMAP_BYTES/sizeof(__u32)];
	struct mlog_rec_tail    mlh_tail;
} __attribute__((packed));

#define MLOG_BITMAP_SIZE(mlh)  ((mlh->mlh_hdr.mrh_len -         \
                                 mlh->mlh_bitmap_offset -       \
                                 sizeof(mlh->mlh_tail)) * 8)

struct mlog_ctxt {
        int                      moc_idx; /* my index the obd array of ctxt's */
        struct mlog_gen          moc_gen;
        struct mtfs_lowerfs     *moc_lowerfs;

        struct mlog_operations  *moc_logops;
        struct mlog_handle      *moc_handle;
        struct mog_canceld_ctxt *moc_llcd;
        struct semaphore         moc_sem; /* protects loc_llcd and loc_imp */
        atomic_t                 moc_refcount;
        //struct mlog_commit_master *moc_lcm;
        void                    *mlog_proc_cb;
        long                     moc_flags; /* flags, see above defines */
};

/** log cookies are used to reference a specific log file and a record therein */
struct mlog_cookie {
	struct mlog_logid       mgc_lgl;
	__u32                   mgc_subsys;
	__u32                   mgc_index;
	__u32                   mgc_padding;
} __attribute__((packed));

struct mlog_handle;

struct plain_handle_data {
        struct list_head    phd_entry;
        struct mlog_handle *phd_cat_handle;
        struct mlog_cookie  phd_cookie; /* cookie of this log in its cat */
        int                 phd_last_idx;
};

struct cat_handle_data {
        struct list_head        chd_head;
        struct mlog_handle     *chd_current_log; /* currently open log */
};

/* In-memory descriptor for a log object or log catalog */
struct mlog_handle {
	struct rw_semaphore     mgh_lock;
	struct mlog_logid       mgh_id;              /* id of this log */
	struct mlog_log_hdr    *mgh_hdr;
	struct file            *mgh_file;
	int                     mgh_last_idx;
	int                     mgh_cur_idx;    /* used during llog_process */
	__u64                   mgh_cur_offset; /* used during llog_process */
	struct mlog_ctxt       *mgh_ctxt;
	union {
		struct plain_handle_data phd;
		struct cat_handle_data   chd;
	} u;
};

/* Log data record types - there is no specific reason that these need to
 * be related to the RPC opcodes, but no reason not to (may be handy later?)
 */
#define MLOG_OP_MAGIC 0x10600000
#define MLOG_OP_MASK  0xfff00000

typedef enum {
        MLOG_PAD_MAGIC   = MLOG_OP_MAGIC | 0x00000,
	MLOG_GEN_REC     = MLOG_OP_MAGIC | 0x40000,
} llog_op_type;

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */


#endif /* __MTFS_LOG_H__ */
