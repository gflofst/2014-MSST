/*
 * Basic data type definitions for IOD(I/O Dispatcher).
 */

#ifndef _IOD_TYPES_H_
#define _IOD_TYPES_H_

#include <stdint.h>
#include "mpi.h"

/**
 * IOD generic handle, which can refer to any local data structure (container,
 * object, event queue ...)
 */
typedef struct {
	uint64_t	cookie;
} iod_handle_t;

typedef MPI_Comm	iod_comm_t;	/** communicator among IODs */
typedef uint64_t	iod_trans_id_t; /** IOD transaction ID */
typedef int		iod_ret_t;	/** return value */
typedef unsigned int	iod_mode_t;

typedef uint64_t	iod_off_t;
typedef uint64_t	iod_size_t;

/** Container open modes */
/** read-only */
#define	IOD_CONT_RO			(1)
/** write-only */
#define	IOD_CONT_WO			(1 << 1)
/** read-write */
#define IOD_CONT_RW			(1 << 2)
/** create container if it's not existed */
#define IOD_CONT_CREATE			(1 << 7)

/** IOD checksum value, 128bits */
typedef struct {
	uint64_t	cs_hi;
	uint64_t	cs_lo;
} iod_checksum_t;

/** IOD object ID */
typedef struct {
	uint64_t	oid_hi;
	uint64_t	oid_lo;
} iod_obj_id_t;

/** IOD object type */
typedef enum {
	IOD_OBJ_ANY,
	IOD_OBJ_ARRAY,
	IOD_OBJ_BLOB,
	IOD_OBJ_KV,
} iod_obj_type_t;

/**
 * IOD array object's dimensions sequence, it determines the layout mapping
 * between logical dimensions and physical layout. The dedault dimensions
 * sequence is same as logical dimension -- the first dimension is the slowest
 * changing dimension and the higher dimensions are faster changing, the last
 * dimension is the fastest changing on disk. User can change the mapping by
 * calling iod_obj_set_layout.
 *
 * For contignous layout, it is an array with num_dims size. For example a 4D
 * array will have uint32_t[4] ([0][1][2][3]) as default dimensions sequence.
 * User can change it as [3][2][1][0] as reverse as logical dimension sequence
 * which means the first logical dimension is the fastest changing on disk and
 * the last logical dimension is the slowest changing dimension. By this simple
 * dimension sequence, user can change the layout mapping from data array's
 * logical multi-dimensional space to physical object's one-dimension adreess
 * space.
 *
 * For chunked layout, the number of chunked dimensions is possibly less than
 * number of original logical dimensions. For example a 3D 2x6x8 data array with
 * 0x6x4 chunk selection will lead to 2D 2x2 chunked dimensions.
 *
 * For extendable array, user cannot change the first dimension's sequence,
 * i.e. the first logical dimension must also be the first physical dimension.
 *
 * IOD will based on this dimension sequence to determine the data location of
 * the logical part of data array. It is only significative for array object,
 * and will be ignored for KV or blob object.
 *
 * The changing of dimension sequence can mainly be used by user for semantic
 * resharding or pre-fetching/replication with uer preferred layout. It is not
 * recommended to change the layout mapping frequently especially on central
 * storage.
 */
typedef uint32_t *	iod_dims_seq_t;

/**
 * Describe multi-dimensional(up to max of 32) data array, allow growth along
 * the first dimension by calling iod_array_extend.
 * For contignous layout array, the chunk_dims should be NULL.
 */
#define IOD_DIMLEN_UNLIMITED	((iod_size_t)(-1))
typedef struct {
	uint32_t        cell_size;       /** cell is the opaque item */
	uint32_t        num_dims;        /** the number of dimensions */
	iod_size_t      *current_dims;   /** the size of each dimension */
	iod_size_t      *chunk_dims;     /** the size of chunk dimension,
					  * NULL for contignous layout. */
	iod_dims_seq_t  dims_seq;        /** dimension sequence, NULL for
					  * default - same as logical */
	iod_size_t      firstdim_max;    /** upper limit on the size of
					  * first dimension */
} iod_array_struct_t;

/**
 * hyperslab region of an IOD data array object, use cell_size and num_dims
 * inside iod_array_struct_t.
 */
typedef struct {
	iod_size_t	*start;
	iod_size_t	*count;
	iod_size_t	*stride;
	iod_size_t	*block;
} iod_hyperslab_t;

/** The location of object */
typedef enum {
	IOD_LOC_BB = 1,		/** object on BB */
	IOD_LOC_CENTRAL,	/** object on central storage */
} iod_location_t;

/**
 * IOD object layout, descripes layout on BB or central storage.
 * loc         -- target location, either IOD_LOC_CENTRAL or IOD_LOC_BB.
 * dims_seq    -- array object's dimension sequence. Set NULL if don't want to
 *		  change it, the previous set dimension sequence will be used.
 *		  It will be ignored for KV or blob object.
 * target_num  -- number of storage targets - DAOS shars or IONs.
 *		  It will be ignored for KV object.
 * stripe_size -- resharding granularity. The count of dataset items for
 *		  contiguous layout array, or the count of chunks for chunked
 *		  layout array. For blob object, the unit is byte.
 *		  All split shards will be round-robin placed on targets.
 *		  It will be ignored for KV object.
 */
typedef struct {
	iod_location_t		loc;
	uint32_t		target_num;
	iod_size_t		stripe_size;
	iod_dims_seq_t		dims_seq;
} iod_layout_t;

/** Key-Value pair */
#define IOD_KV_KEY_MAXLEN	(256)
#define IOD_KV_VALUE_MAXLEN	(64*1024)
typedef struct {
	const char	*key;		/** must be NULL termed */
	const void	*value;
	iod_size_t	value_len;
} iod_kv_t;

/**
 * hints for possible performance optimization.
 */
#define IOD_HINTKEY_MAXLEN	(128)
#define IOD_HINTVAL_MAXLEN	(128)
typedef struct {
	const char	*key;		/** must be NULL termed */
	const char	*value;		/** must be NULL termed */
} iod_hint_t;

/** pack set of hints into one struct */
typedef struct {
	iod_size_t	num_hint;	/** number of hints */
	iod_hint_t	hint[0];	/** hint list */
} iod_hint_list_t;

/** IOD memroy buffer fragment */
typedef struct {
	void		*addr;
	iod_size_t	len;
} iod_mem_frag_t;

/**
 * IOD memory descriptor, it's an array of iod_mem_frag_t and it's source of
 * write or target of read.
 */
typedef struct {
	unsigned long	nfrag;
	iod_mem_frag_t	frag[0];	/* C99 extension */
} iod_mem_desc_t;

/** IO fragment of a IOD object */
typedef struct {
	iod_off_t	offset;
	iod_size_t	len;
} iod_blob_iofrag_t;

/**
 * IO desriptor of IOD object offset I/O, it's an array of iod_blob_iofrag_t
 * and it's the target of write or source of read.
 */
typedef struct {
	unsigned long		nfrag;
	iod_blob_iofrag_t	frag[0];
} iod_blob_iodesc_t;

/**
 * IO desriptor of IOD array object, use iod_hyperslab_t to descript the
 * sub-dataspace. It's the target of write or source of read.
 */
typedef iod_hyperslab_t		iod_array_iodesc_t;

/** Transaction I/O modes */
/** read-only */
#define	IOD_TRANS_RD			(1)
/** write-only */
#define IOD_TRANS_WR			(1 << 1)

/**
 * The statuses of IOD transaction.
 * Invalid  - this TID has not been started,
 * Started  - has been started but some writers have not finished it,
 * Aborted  - being aborted by user,
 * Finished - all writers have called finish but earlier transactions are not
 *	      finished,
 * Readable - all writers have called finish and all earlier transactions are
 *	      also finished,
 * Durable  - readable and the migration to central is finished,
 */
typedef enum {
	IOD_TRANS_INVALID = 0,
	IOD_TRANS_STARTED,
	IOD_TRANS_ABORTED,
	IOD_TRANS_FINISHED,
	IOD_TRANS_READABLE,
	IOD_TRANS_DURABLE,
} iod_trans_status_t;

/**
 * The TIDs' status of one IOD container.
 * For read, IOD can provide a consistent view between lowest_durable and
 * latest_rdable, with possible un-used TIDs between them.
 * For write, upper layer should provide a tid greater than latest_wrting.
 */
typedef struct {
	/** the lowest durable TID on DAOS */
	iod_trans_id_t		lowest_durable;
	/** the latest readable TID on BB */
	iod_trans_id_t		latest_rdable;
	/** the latest tid in writing (started with IOD_TRANS_WR) */
	iod_trans_id_t		latest_wrting;
} iod_container_tids_t;

#define IOD_TID_UNKNOWN		((iod_trans_id_t)(-1))
#define IOD_TRANS_ABORT_ALL	(1)
#define IOD_TRANS_ABORT_SINGLE	(2)

/**
 * Event status, when ev_status == IOD_EVS_COMPLETED, the rc inside the event
 * determine the completion status of the operation: rc==0 stands for success
 * finish, rc<0 stands for error code for that completed operation.
 */
typedef enum {
	IOD_EVS_INIT,
	IOD_EVS_INFLIGHT,
	IOD_EVS_COMPLETED,
	IOD_EVS_ABORTED,
	IOD_EVS_FINI,
} iod_ev_status_t;

/** Event type */
typedef enum {
	IOD_EV_SYS_INIT,
	IOD_EV_SYS_FINI,
	IOD_EV_CONT_OPEN,
	IOD_EV_CONT_CLOSE,
	IOD_EV_CONT_UNLINK,
	IOD_EV_CONT_LS_OBJ,
	IOD_EV_CONT_QUERY_TIDS,
	IOD_EV_CONT_SNAPSHOT,
	IOD_EV_OBJ_CREATE,
	IOD_EV_OBJ_OPEN_WR,
	IOD_EV_OBJ_OPEN_RD,
	IOD_EV_ARR_RD,
	IOD_EV_ARR_WR,
	IOD_EV_BLOB_RD,
	IOD_EV_BLOB_WR,
	IOD_EV_ARR_GET_STRUCT,
	IOD_EV_ARR_EXT,
	IOD_EV_OBJ_SET_LAYOUT,
	IOD_EV_OBJ_GET_LAYOUT,
	IOD_EV_OBJ_UNLINK,
	IOD_EV_OBJ_SET_SCRA,
	IOD_EV_OBJ_GET_SCRA,
	IOD_EV_OBJ_PURGE,
	IOD_EV_OBJ_FETCH,
	IOD_EV_OBJ_REPLICA,
	IOD_EV_TRANS_QUERY,
	IOD_EV_TRANS_START,
	IOD_EV_TRANS_SLIP,
	IOD_EV_TRANS_FINISH,
	IOD_EV_TRANS_PERSIST,
	IOD_EV_KV_SET,
	IOD_EV_KV_GET_NUM,
	IOD_EV_KV_GET,
	IOD_EV_KV_LIST_KEY,
	IOD_EV_KV_GET_VALUE,
	IOD_EV_KV_UNLINK_KEY,
	IOD_EV_EQ_DESTROY,
} iod_ev_type_t;

/** wait for completion event forever */
#define IOD_EQ_WAIT            -1
/** always return immediately */
#define IOD_EQ_NOWAIT          0

typedef enum {
	/* query outstanding completed event */
	IOD_EVQ_COMPLETED	= (1),
	/* query # inflight event */
	IOD_EVQ_INFLIGHT	= (1 << 1),
	/* query # aborted event */
	IOD_EVQ_ABORTED		= (1 << 2),
	/* query # inflight + completed events in EQ */
	IOD_EVQ_ALL		= (IOD_EVQ_COMPLETED | IOD_EVQ_INFLIGHT
				   | IOD_EVQ_ABORTED),
} iod_ev_query_t;

struct iod_event;
typedef int (*iod_event_callback_t) (struct iod_event *);
typedef struct iod_event{
	iod_ev_type_t		ev_type;
	iod_ev_status_t		ev_status;
	int			rc;		/** return code */
	iod_event_callback_t	*cb_fn;		/** upper layer registered */
	void			*cb_data;	/** upper layer registered */
	void			*opaque;	/** used by IOD internally */
} iod_event_t;

/**
 * The below are some structures defined just for packing some IN/OUT parameters
 * together to facilitate the parameters passing.
 */

/**
 * This struct is used only for packing object create's parameters.
 * The \a name will be ignored by KV type object as KV objects are nameless.
 * The \a array_struct is only meaningful for array object.
 */
#define IOD_OBJ_NAME_MAXLEN	(256)
typedef struct {
	iod_obj_type_t      type;	  /** IN whether KV, ARRAY, or BLOB */
	const char          *name;        /** IN can be NULL for nameless obj */
	iod_array_struct_t  *array_struct;/** IN object struct for ARRAY */
	iod_hint_list_t     *hints;       /** IN optional hints */
	iod_obj_id_t        *oid;         /** OUT the object ID */
	iod_ret_t           *ret;         /** OUT return value */
} iod_obj_create_t;

/** Used only for the iod_obj_set/get_scratch_list function. */
typedef struct {
	iod_handle_t        oh;        /** IN object handle */
	char                *scratch;  /** IN/OUT 32 bytes scratchpad pointer */
	iod_checksum_t      *cs;       /** IN passed in checksum for set, or
					*  OUT returned checksum for get */
	iod_ret_t           *ret;      /** OUT return value */
} iod_obj_scratch_t;

/** Used only for iod_obj_open_write_list() parameters packing */
typedef struct {
	iod_obj_id_t        oid;       /** IN object ID */
	iod_hint_list_t     *hints;    /** IN hints pointer */
	iod_handle_t        *oh;       /** OUT object handle */
	iod_ret_t           *ret;      /** OUT return value */
} iod_obj_open_t;

/** Used only for iod_obj_write/read_struct_list() parameters packing */
typedef struct {
	iod_handle_t        oh;        /** IN object handle */
	iod_hint_list_t     *hints;    /** IN hints pointer */
	iod_mem_desc_t      *mem_desc; /** IN memory descriptor */
	iod_array_iodesc_t  *io_desc;  /** IN hyperslab pointer */
	iod_checksum_t      *cs;       /** IN passed in checksum for write, or
					*  OUT returned checksum for read */
	iod_ret_t           *ret;      /** OUT return value */
} iod_array_io_t;

/** Used only for iod_blob_write/read_list() parameters packing */
typedef struct {
	iod_handle_t        oh;        /** IN object handle */
	iod_hint_list_t     *hints;    /** IN hints pointer */
	iod_mem_desc_t      *mem_desc; /** IN memory descriptor */
	iod_blob_iodesc_t   *io_desc;  /** IN offset I/O descriptor */
	iod_checksum_t      *cs;       /** IN passed in checksum for write, or
					*  OUT returned checksum for read */
	iod_ret_t           *ret;      /** OUT return value */
} iod_blob_io_t;

/**
 * Used only for packing the iod_obj_close_list's parameters.
 */
typedef struct {
	iod_handle_t        oh;        /** IN object handle */
	iod_hint_list_t     *hints;    /** IN optional hints */
	iod_ret_t           *ret;      /** OUT return value */
} iod_obj_close_t;

/**
 * Used only for packing the iod_obj_get_struct_list's parameters.
 */
typedef struct {
	iod_handle_t        oh;	         /** IN object handle */
	iod_array_struct_t  *obj_struct; /** OUT returned object struct */
	iod_ret_t           *ret;        /** OUT return value */
} iod_array_get_struct_t;

/**
 * Used only for packing the iod_obj_extend_list's parameters.
 */
typedef struct {
	iod_handle_t        oh;           /** IN object handle */
	iod_size_t          firstdim_len; /** IN new first dim length */
	iod_ret_t           *ret;         /** OUT return value */
} iod_obj_extend_t;

/**
 * Used only for packing the iod_obj_unlink_list's parameters.
 */
typedef struct {
	iod_obj_id_t        oid;         /** IN object ID */
	iod_ret_t           *ret;        /** OUT return value */
} iod_obj_unlink_t;

/**
 * Used only for packing the iod_kv_set_list's parameters.
 */
typedef struct {
	iod_kv_t            *kv;         /** In KV pair's pointer */
	iod_checksum_t      *cs;         /** IN passed in checksum for set, or
					  *  OUT returned checksum for get */
	iod_ret_t           *ret;        /** OUT return value */
} iod_kv_params_t;

#endif /* _IOD_TYPES_H_ */
