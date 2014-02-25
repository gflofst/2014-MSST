/*
 * The draft API for the IO Dispatcher (IOD).
 * Last modified:  Feb 20, 2013  11:00AM
 */

#ifndef _IOD_API_H_
#define _IOD_API_H_

#include "iod_types.h"

/**
 * Instantiate IOD service.
 *
 * It should be called collectively by every application process at initializing
 * phase.
 *
 * The \a total_cnranks and \a cnranks are for IOD optimization. For example
 * if one TID's num_ranks == total_cnranks it means that all CN ranks
 * participate it so IOD can use collective instead lots of P2P messages to
 * synchronize transaction status; the cnranks can help IOD to know how many
 * CN ranks are connected to it.
 * User can pass in zero if it does not know it, but \a total_cnranks and
 * \a cnranks should be provided with meaningful value or set as zero
 * simultaneously. If they are provided, for application with dynamic processes,
 * user should re-call this routine to notify IOD the new value after dynamic
 * processes are created.
 *
 * \param comm [IN]		Global MPI communicator among all IODs
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param total_cnranks[IN]	total number of CN ranks in application
 * \param cnranks[IN]		number of CN ranks that connect to this IOD
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_initialize(iod_comm_t comm, iod_hint_list_t *hints,
	       unsigned int total_cnranks, unsigned int cnranks,
	       iod_event_t *event);

/**
 * Finalize IOD for resource freeing.
 *
 * It should be called collectively by every application process when user
 * program exiting.
 *
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_finalize(iod_hint_list_t *hints, iod_event_t *event);


/* SECTION 1 ***** CONTAINER FUNCTIONS ********************/

/**
 * Open an IOD container.
 *
 * It should be collectively called by every application process.
 * IOD will call daos_container_open to create/open DAOS container.
 *
 * \param path [IN]	container path name
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param mode [IN]     open mode, can be IOD_CONT_RO, IOD_CONT_WO or
 *			IOD_CONT_RW. For IOD_CONT_WO or IOD_CONT_RW can also
 *			carry a IOD_CONT_CREATE flag for creating new container.
 *			It is recommended that caller should use read-only or
 *			write-only mode if it needs not writing or reading.
 *			Always pass in IOD_CONT_RW will lose some internal
 *			optimization in IOD layer.
 * \param coh [IN/OUT]	returned container handle
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_container_open(const char *path, iod_hint_list_t *hints, unsigned int mode,
		   iod_handle_t *coh, iod_event_t *event);

/**
 * Close an IOD container.
 *
 * It should be collectively called by every application process.
 * All opened and un-closed IOD container will be implicitly closed inside
 * iod_finalize.
 *
 * \param coh [IN]	container handle
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_container_close(iod_handle_t coh, iod_hint_list_t *hints,
		    iod_event_t *event);

/**
 * Unlink an IOD container by path.
 *
 * Any single process can call it. Caller should ensure the container is closed
 * before calling _unlink, it will fail if unlinks an un-closed container.
 *
 * \param path [IN]	container path name
 * \param force [IN]	If not zero then forcibly unlink even if container is
 *			non-empty. If set as zero then will return -ENOTEMPTY
 *			for non-empty container.
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_container_unlink(const char *path, int force, iod_event_t *event);

/**
 * Get list of objects within an IOD container at a particular TID.
 *
 * It can be called by any processes, commonly should be called by single
 * process as multiple processes' concurrent calling possibly will lead to
 * performance downgrade.
 *
 * The \a tid can be gotten from iod_container_query_tids, or set as zero which
 * means to list this container's latest readable tid.
 *
 * \param coh [IN]	container handle
 * \param tid [IN]	transaction ID
 * \param filter [IN]	object type filter, only list this type object.
 *			if it is set as IOD_OBJ_ANY, then will first list ARRAY
 *			then BLOB and KV objects.
 * \param offset [IN]	offset of object ID in list, ordered by ID value from
 *			lower to higher.
 * \param num [IN]	how many objects to list
 * \param oid [IN/OUT]	returned object ID list, user provides the memory.
 * \param type [IN/OUT]	returned object type list, user provides the memory.
 * \param name [IN/OUT]	object name, can be empty for anonymous object.
 *			Caller should provide the buffer list for name (with
 *			length as IOD_OBJ_NAME_MAXLEN), the first name's buffer
 *			address is (name), the second is (name +
 *			IOD_OBJ_NAME_MAXLEN) and so on.
 *			Can be NULL if caller does not care the name.
 * \param event [IN]	pointer to completion event
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_container_list_obj(iod_handle_t coh, iod_trans_id_t tid,
		       iod_obj_type_t filter, iod_off_t offset, iod_size_t num,
		       iod_obj_id_t *oid, iod_obj_type_t *type, char *name,
		       iod_event_t *event);


/* SECTION 2 ***** OBJECT FUNCTIONS ********************/

/**
 * Create one IOD object.
 *
 * Only single process can call it. Common usage is one rank creates object and
 * shares the returned object ID to other ranks.
 *
 * Permissions inherited from parent container, when creates array object it
 * will set the array's default dimension sequence(same as logical dimension).
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param type[IN]		object type
 * \param name[IN]		optional object name, will be ignored for KV
 *				object as KV object is nameless.
 * \param array_struct[IN]	array structure, only meaningful for array.
 * \param oid[OUT]		returned object ID
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_create(iod_handle_t coh, iod_trans_id_t tid, iod_hint_list_t *hints,
	       iod_obj_type_t type, const char *name,
	       iod_array_struct_t *array_struct, iod_obj_id_t *oid,
	       iod_event_t *event);

/**
 * Create a set of IOD objects within one transaction.
 * It is a wrapper inside which looply calls iod_obj_create.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects to create
 * \param obj_create [IN/OUT]	object create structure list pointer
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_create_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		    iod_obj_create_t *obj_create, iod_event_t *event);

/**
 * Open one IOD object for writing.
 *
 * Every rank which wants to do I/O for the object should call it to get a
 * opened object handle.
 *
 * The iod_obj_create will set the object's default layout, after open
 * the caller can change it by calling iod_obj_set_layout.
 *
 * \param coh [IN]	container handle
 * \param oid [IN]	object ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param oh [IN/OUT]	object handle pointer
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_obj_open_write(iod_handle_t coh, iod_obj_id_t oid, iod_hint_list_t *hints,
		   iod_handle_t *oh, iod_event_t *event);

/**
 * Open a set of IOD objects for writing.
 * It is a wrapper inside which looply calls iod_obj_open_write.
 *
 * \param coh [IN]		container handle
 * \param num [IN]		number of objects to open
 * \param open_write [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_open_write_list(iod_handle_t coh, iod_size_t num,
			iod_obj_open_t *open_write, iod_event_t *event);

/**
 * Write to one IOD array object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param mem_desc [IN]	pointer to memory buffers descriptor
 * \param io_desc[IN]	pointer to io descripter
 * \param cs[IN]	passed in checksum pointer for the write
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_array_write(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		iod_mem_desc_t *mem_desc, iod_array_iodesc_t *io_desc,
		iod_checksum_t *cs, iod_event_t *event);

/**
 * Write to a set of IOD array objects within one transaction.
 * It is a wrapper inside which looply calls iod_obj_write_struct.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		number of objects for writing
 * \param array_write [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_array_write_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		     iod_array_io_t *array_write, iod_event_t *event);

/**
 * Write to one IOD blob object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param mem_desc [IN]	pointer to memory buffers descriptor
 * \param io_desc [IN]	pointer to offset I/O descriptor
 * \param cs[IN]	passed in checksum pointer for the write
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_blob_write(iod_handle_t oh, iod_trans_id_t tid,
	       iod_hint_list_t *hints, iod_mem_desc_t *mem_desc,
	       iod_blob_iodesc_t *io_desc, iod_checksum_t *cs,
	       iod_event_t *event);

/**
 * write to a set of IOD blob objects within one transaction.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		number of objects to write
 * \param blob_write [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_blob_write_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		    iod_blob_io_t *blob_write, iod_event_t *event);

/**
 * Open one IOD object for read.
 *
 * Every rank which wants to do I/O for the object should call it to get a
 * opened object handle.
 *
 * \param coh [IN]	container handle
 * \param oid [IN]	object ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param oh [OUT]	object handle pointer
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_obj_open_read(iod_handle_t coh, iod_obj_id_t oid, iod_hint_list_t *hints,
		  iod_handle_t *oh, iod_event_t *event);

/**
 * Open a set of IOD objects for read.
 * It is a wrapper inside which looply calls iod_obj_open_read.
 *
 * \param coh [IN]		container handle
 * \param num [IN]		how many object to read
 * \param open_read [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_open_read_list(iod_handle_t coh, iod_size_t num,
		       iod_obj_open_t *open_read, iod_event_t *event);

/**
 * Read from one IOD array object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param mem_desc [IN]	pointer to memory buffers descriptor
 * \param io_desc[IN]	pointer to I/O descriptor
 * \param cs[IN/OUT]	returned checksum for the read
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_array_read(iod_handle_t oh, iod_trans_id_t tid,
	       iod_hint_list_t *hints, iod_mem_desc_t *mem_desc,
	       iod_array_iodesc_t *io_desc, iod_checksum_t *cs,
	       iod_event_t *event);

/**
 * Read from a set of IOD array objects within one transaction.
 * It is a wrapper inside which looply calls iod_obj_read_struct.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects to read
 * \param array_read [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_array_read_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		    iod_array_io_t *array_read, iod_event_t *event);

/**
 * Read from one IOD blob object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param mem_desc [IN]	pointer to memory buffers descriptor
 * \param io_desc [IN]	pointer to blob I/O descriptor
 * \param cs[IN/OUT]	returned checksum for the read
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_blob_read(iod_handle_t oh, iod_trans_id_t tid,
	      iod_hint_list_t *hints, iod_mem_desc_t *mem_desc,
	      iod_blob_iodesc_t *io_desc, iod_checksum_t *cs,
	      iod_event_t *event);

/**
 * Read from a set of IOD blob objects within one transaction.
 *
 * \param tid [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects to read
 * \param blob_read [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_blob_read_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		   iod_blob_io_t *blob_read, iod_event_t *event);

/* needs iod_blob_query_len() or iod_blob_stat()? */

/**
 * Close one IOD object.
 *
 * Every rank which has opened object should close it at appropriate time.
 * User should call close in time when this rank does not intend to do I/O for
 * that object, this can facilitate IOD's internal optimization and resource
 * freeing.
 *
 * \param oh [OUT]	object handle
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_obj_close(iod_handle_t oh, iod_hint_list_t *hints, iod_event_t *event);

/**
 * Close a set of IOD objects within one container.
 * It is a wrapper inside which looply calls iod_obj_close.
 *
 * \param coh [IN]		container handle
 * \param num [IN]		how many objects to close
 * \param obj_close [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_close_list(iod_handle_t coh, iod_size_t num, iod_obj_close_t *obj_close,
		   iod_event_t *event);

/**
 * Get current structure(dimensionality and cellsize) of an IOD ARRAY object.
 *
 * Any rank can call it, commonly single rank calls it to query.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param obj_struct [IN/OUT]	pointer of object structure
 * \param event [IN]		pointer of completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_array_get_struct(iod_handle_t oh, iod_trans_id_t tid,
		     iod_array_struct_t *array_struct, iod_event_t *event);

/**
 * Get current structure(dimensionality and cellsize) of a set of IOD objects
 * within one transaction.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects
 * \param get_struct [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_array_get_struct_list(iod_handle_t coh, iod_trans_id_t tid,
			  iod_size_t num, iod_array_get_struct_t *get_struct,
			  iod_event_t *event);

/**
 * Extend one IOD ARRAY object's dimensionality.
 * One data array object can be extended along the first dimension.
 * It will fail when the object handle has no writing permission.
 * It will fail for fixed dimensional array, or \a firstdim_len >
 * firstdim_max.
 *
 * Only single rank should call it to extend array object. Caller should aware
 * the possible race here: when one rank extends array, previous gotten array
 * structure (by calling iod_array_get_struct) by other ranks will be invalid.
 * TODO: needs to support shink?
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param firstdim_len[IN]	the first dimension length
 * \param event [IN]		completion event pointer list
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_array_extend(iod_handle_t oh, iod_trans_id_t tid, iod_size_t firstdim_len,
		 iod_event_t *event);

/**
 * Extend the dimensionality of a set of IOD ARRAY objects.
 * All the objects should in one container. It will fail when the object handle
 * has no writing permission.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects
 * \param obj_extend [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_array_extend_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		      iod_obj_extend_t *obj_extend, iod_event_t *event);

/**
 * Set one IOD object's layout on storage system.
 *
 * This routine is only for setting object's layout on BB or central storage.
 * It will not start/trigger data movement. Only single rank should call it.
 *
 * Common usage is user can first set the layout, when iod_trans_persist IOD
 * will based on this setted layout to determine data placement when migrating
 * this object to DAOS. At that time IOD will migrate object's data with the
 * dimension sequence of \a layout.dims_seq, all split shards will be
 * round-robin placed on \a layout.target_num's targets with stripe size of
 * \a layout.stripe_size.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param layout [IN]		passed in layout of the object
 * \param event [IN]		completion event pointer list
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_set_layout(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		   iod_layout_t *layout, iod_event_t *event);

/**
 * Query one IOD object's layout on storage system.
 *
 * Any rank can call it, commonly single rank calls it to query.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID, must be readable
 * \param layout [IN/OUT]	returned object layout
 * \param event [IN]		completion event pointer list
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_get_layout(iod_handle_t oh, iod_trans_id_t tid, iod_layout_t *layout,
		   iod_event_t *event);

/**
 * Unlink one IOD object by ID.
 *
 * Caller should close it before unlink, it will fail if that object is in open.
 * Only single rank should call it.
 *
 * \param coh [IN]	container handle
 * \param oid [IN]	object ID
 * \param tid [IN]	transaction ID, object will be un-existed at TIDs equal
 *			or greater than this \a tid, but will keep existed at
 *			TIDs lower than this \a tid.
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_obj_unlink(iod_handle_t coh, iod_obj_id_t oid, iod_trans_id_t tid,
	       iod_event_t *event);

/**
 * Unlink a set of IOD objects by IDs, all in one transaction.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects to unlink
 * \param obj_unlink [IN/OUT]	pointer list of packed parameters
 * \param event [IN]		pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_obj_unlink_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
		    iod_obj_unlink_t *obj_unlink, iod_event_t *event);

/**
 * Set the 32 bytes scratchpad of an object.
 * IOD supports one 32 bytes length scratchpad for every object. User can use
 * it to store object's child-object ID or something else. For IOD, it is just
 * 32 bytes stream.
 *
 * It will fail if the object handle has no writing permission.
 * Any rank can call it, commonly single rank calls it to set.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param scratch[IN]		passed in 32bytes stream of scratchpad
 * \param cs[IN]		passed in checksum pointer for the scratch set
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_set_scratch(iod_handle_t oh, iod_trans_id_t tid, const char* scratch,
		    iod_checksum_t *cs, iod_event_t *event);

/**
 * Set the 32 bytes scratchpad for a set of objects.
 * It is a wrapper inside which looply calls iod_obj_set_scratch.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects to unlink
 * \param scratch_set[IN/OUT]	pointer to iod_obj_scratch_t list
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_set_scratch_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
			 iod_obj_scratch_t *scratch_set, iod_event_t *event);

/**
 * Get the 32 bytes scratchpad of an object.
 * IOD supports one 32 bytes length scratchpad for every object. User can use
 * it to store object's child-object ID or something else. For IOD, it is just
 * 32 bytes stream.
 *
 * Any rank can call it, commonly single rank calls it to get.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param scratch[IN/OUT]	32bytes stream of scratchpad
 * \param cs[IN/OUT]		returned checksum for the scratchpad
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_get_scratch(iod_handle_t oh, iod_trans_id_t tid, const char* scratch,
		    iod_checksum_t *cs, iod_event_t *event);

/**
 * Get the 32 bytes scratchpad for a set of objects.
 * It is a wrapper inside which looply calls iod_obj_get_scratch.
 *
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		transaction ID
 * \param num [IN]		how many objects to unlink
 * \param scratch_get[IN/OUT]	pointer to iod_obj_scratch_t list
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_get_scratch_list(iod_handle_t coh, iod_trans_id_t tid, iod_size_t num,
			 iod_obj_scratch_t *scratch_get, iod_event_t *event);


/* SECTION 3 ********** DATA CONSISTENCY *************************/

/**
 * Query one IOD container's TIDs status.
 *
 * Any rank can call it, commonly single rank calls it to query and can share
 * it to other ranks.
 *
 * Caller should be aware that there could be race condition here - when this
 * call returns, the underneath TID status possibly being changed as other work
 * is ongoing.
 *
 * \param coh [IN]		container handle
 * \param tids [IN/OUT]		pointer to container tid status
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_container_query_tids(iod_handle_t coh, iod_container_tids_t *tids,
			 iod_event_t *event);

/**
 * Query one TID's status.
 *
 * Caller should be aware that there could be race condition here - when this
 * call returns, the underneath TID status possibly being changed as other work
 * is ongoing.
 *
 * \param coh [IN]		container handle
 * \param tid [IN]		TID to query
 * \param status [IN/OUT]	pointer to transaction status
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_trans_query(iod_handle_t coh, iod_trans_id_t tid,
		iod_trans_status_t *status, iod_event_t *event);

/**
 * Start a new transaction for an IOD container.
 *
 * User should start a transaction for either writing(\a mode == IOD_TRANS_WR)
 * or reading(\a mode == IOD_TRANS_RD). IOD does not allow to read and write a
 * TID at the same time. iod_container_query_tids can help to get an appropriate
 * TID for writing or reading, user also can pass in IOD_TID_UNKNOWN as
 * explained below.
 *
 * For writing, user should pass in an appropriate TID, for the same TID all
 * callers must call it with same \a num_ranks. User can get the appropriate
 * TID by either:
 * 1) calling iod_container_query_tids, the TID higher than latest_wrting is
 *    writable - common use case is to start (latest_wrting+1); or
 * 2) passing in IOD_TID_UNKNOWN, IOD will set the \a tid as (latest_wrting+1)
 *    and allow user to write to it.
 * If two independent groups need to start new transaction for writing, then
 * commonly they should pass in IOD_TID_UNKNOWN to avoid TID confliction.
 *
 * For reading, user should pass in an appropriate TID, for the same TID all
 * callers must call it with same \a num_ranks. User can get the appropriate
 * TID by either:
 * 1) calling iod_container_query_tids, IOD can ensure the read consistency of
 *    TIDs between lowest_durable and latest_rdable (note that there might be
 *    some aborted/invalid TIDs which are not readable); or
 * 2) passing in IOD_TID_UNKNOWN, IOD will set the \a tid as latest_rdable and
 *    allow user to read from it.
 *    Hints: key "lowest_readable" value "true" means start by lowest readable
 *           TID. For example, on BB there are readable TIDs 8, 9, 10, 11. This
 *           hint will cause start by TID 8.
 *	     If no hints or other invalid hints then IOD will slip to latest
 *	     readable TID. In the above example will start by TID 11.
 *
 * A ref-count will be taken when this call finishes sucessfully, and will be
 * released after iod_trans_finish or iod_trans_slip's sucess completion.
 *
 * \a num_ranks is the number of participators for this transaction. IOD
 * supports two kinds of transaction status synchronization and consistency
 * ensuring mechanism:
 * 1) application does the transaction status synchronization and consistency
 *    ensuring. CN ranks will need to select one transaction leader. The leader
 *    starts and finishes/slips the TID, other ranks can participate in the TID
 *    after leader having started it, and the leader should ensure all other
 *    ranks have finished I/O operations within this TID before it finish/slip
 *    this TID. This is the method similar as DAOS epoch's requirement.
 *    User should pass in \a num_ranks as zero for this method.
 * 2) IOD internally does the transaction status synchronization and consistency
 *    ensuring. CN ranks only need to start and finish/slip this TID seperately
 *    and independently. IOD will do lots of internal P2P message passing for
 *    status synchronization. As IOD does not know CN ranks' process group
 *    information, so IOD cannot use group collective communication. When
 *    \a num_ranks is large, the status synchronizations will introduce
 *    considerable overhead and latency at IOD layer.
 *    User should pass in \a num_ranks as the number of CN ranks that will
 *    participate in this transaction for this method. IOD_TID_UNKNOWN cannot be
 *    used for this method.
 * Commonly application should use method 1) to participate transaction.
 *
 * \param coh [IN]		container handle
 * \param tid [IN/OUT]		pointer to transaction ID
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param num_ranks[IN]		count of participators(CN ranks)
 * \param mode[IN]		either IOD_TRANS_RD or IOD_TRANS_WR,
 *				will return -EINVAL if passes in other value.
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_trans_start(iod_handle_t coh, iod_trans_id_t *tid, iod_hint_list_t *hints,
		unsigned int num_ranks, unsigned int mode,
		iod_event_t *event);

/**
 * Slip a transaction for an IOD container.
 *
 * The slip is like a combination of "iod_trans_finish(old_tid) +
 * iod_trans_start(new_tid)". User should pass in an determined and appropriate
 * \a tid to slip(cannot be IOD_TID_UNKNOWN). Slip can only be done when the
 * num_ranks is same between old_tid and new_tid, or user should call
 * iod_trans_finish(old_tid) and iod_trans_start(new_tid) seperately with the
 * different num_ranks.
 *
 * If \a tid is for writing, IOD will do "iod_trans_finish(*tid) +
 * iod_trans_start(next_writable_tid)". After completion, *tid is set as next
 * writable tid which commonly is (*tid + 1).
 *
 * If \a tid is for reading, IOD will do "iod_trans_finish(*tid) +
 * iod_trans_start(next_readable_tid)". After completion, *tid is set as next
 * readable tid.
 * Hints: key "adjacent_readable" value "true" means slip to adjacent readable
 *        TID. For example, current (*tid) is 8, and have higher readable TIDs
 *        9, 10, 11. This hints will cause slip to TID 9.
 *	  If no hints or other invalid hints then IOD will slip to latest
 *	  readable TID. In the above example will slip to TID 11.
 *	  This hints is only meaningful for reading.
 *
 * \param coh [IN]		container handle
 * \param tid [IN/OUT]		pointer to transaction ID
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_trans_slip(iod_handle_t coh, iod_trans_id_t *tid, iod_hint_list_t *hints,
	       iod_event_t *event);

/**
 * Finish a transaction for an IOD container.
 *
 * For writing, if one tid's all participators have completed the
 * iod_trans_finish, and this tid's all former started tids have become
 * readable or aborted, then this tid become readable in burst buffer. The
 * success completion of iod_trans_finish means this \a tid become readable
 * on BB. However, to ensure one tid's data to be durable on central storage,
 * user should call iod_trans_persist to migrate/protect it.
 *
 * The \a abort is to abort an transaction, all writings/updatings in this
 * transaction will be rolled back after success aborting.
 *
 * For reading, this routine is only for releasing the ref-count taken by
 * iod_trans_start(or _slip).
 *
 * \param coh [IN]	container handle
 * \param tid [IN]	transaction ID
 * \param abort [IN]	abort the transaction, only meaningful for writing.
 *			1) Zero for commonly finish;
 *			2) IOD_TRANS_ABORT_ALL for aborting this TID and all
 *			   higher TIDs in writing;
 *			3) IOD_TRANS_ABORT_SINGLE for aborting only this TID.
 *			Other value will return -EINVAL.
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_trans_finish(iod_handle_t coh, iod_trans_id_t tid, iod_hint_list_t *hints,
		 int abort, iod_event_t *event);


/**
 * Persist/protect a transaction for an IOD container.
 *
 * Only single rank should call this routine to make this transaction be
 * migrated to central storage. User can only persist a readable TID.
 *
 * IOD will make all lower readable TIDs and this TID's state be persist. If
 * user want to set/change some objects' layout on central storage, it should
 * call iod_obj_set_layout before calling iod_trans_persist. After persist's
 * completion this \a tid become durable on central storage. The previous
 * readable TID is still on BB, IOD will not automatically purge it.
 *
 * \param coh [IN]	container handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_trans_persist(iod_handle_t coh, iod_trans_id_t tid, iod_hint_list_t *hints,
		  iod_event_t *event);

/**
 * Purge a object for an IOD container from BB.
 *
 * Only single rank can call this routine to purge one object from BB.
 * This is commonly used after migrating done and that object of \a tid is
 * un-needed to be kept on BB. IOD won't do auto-purging, user should explicitly
 * call the purge at appropriate time to free BB's storage space. Purging is at
 * object level, IOD will purge all lower TIDs and this TID \a tid.
 *
 * User can only call purge with the \a tid that ever passed in by
 * iod_trans_persist. If it is called before the completion of iod_trans_persist
 * IOD will delay the real purging after the success completion of
 * iod_trans_persist.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_obj_purge(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
	      iod_event_t *event);

/**
 * Fetch/pre-stage one object from central storage to BB.
 *
 * Only single rank can call this routine to fetch one object from central
 * storage to BB.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param slab [IN]		the hyperslab within this object
 *				will be ignored for KV object.
 * \param layout [IN]		passed in layout placement on BB,
 *				can be NULL if does not change layout on BB.
 * \param new_tid[IN/OUT]	returned new TID, will be same as passed in
 *				\a tid if does not change the \a layout on BB.
 * \param event [IN]		completion event pointer list
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_fetch(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
	      iod_hyperslab_t *slab, iod_layout_t *layout,
	      iod_trans_id_t *new_tid, iod_event_t *event);

/**
 * Create a replica for one object from BB to BB.
 *
 * Only single rank can call this routine to create a replica for existed object
 * on BB. Later user should explicitly call iod_obj_purge to purge this replica
 * when it is not needed.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param this_only [IN]	un-zero means only create a replica for this TID
 *				incremented data; Zero means replicate all TIDs'
 *				data till this \a tid.
 * \param layout [IN]		passed in layout placement on BB, if it is same
 *				as previous layout IOD will not do anything.
 * \param new_tid[IN/OUT]	returned new TID, will be same as passed in
 *				\a tid if does not change the \a layout on BB.
 * \param event [IN]		completion event pointer list
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_obj_replica(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		unsigned int this_only, iod_layout_t *layout,
		iod_trans_id_t *new_tid, iod_event_t *event);

/**
 * Create snapshot for a IOD container based on its latest readable status.
 *
 * Only single rank can call this routine to create a container snapshot.
 * IOD will migrate latest readable TID to DAOS and create DAOS container
 * snapshot by calling daos_container_snapshot.
 *
 * \param coh [IN]		container handle
 * \param snapshot [IN]		name of snapshot
 * \param hints[IN]		pointer to hints and can be NULL when no hint
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_container_snapshot(iod_handle_t coh, const char *snapshot,
		       iod_hint_list_t *hints, iod_event_t *event);


/* SECTION 4 ********** KEY-VALUE OPERATIONS *************************/

/**
 * Set one Key-Value pair to one KV object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param kv [IN]	KV pair pointer
 * \param cs[IN]	passed in checksum pointer for the KV pair
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_kv_set(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
	   iod_kv_t *kv, iod_checksum_t *cs, iod_event_t *event);

/**
 * Set a list of Key-Value pairs to one KV object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param num [IN]	number of KV pairs
 * \param kvs [IN/OUT]	pointer to KV parameters packet list
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_kv_set_list(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		iod_size_t num, iod_kv_params_t *kvs, iod_event_t *event);

/**
 * Get the number of Key-Value pairs of one KV object.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param num [IN/OUT]	pointer of number of KV pairs
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_kv_get_num(iod_handle_t oh, iod_trans_id_t tid, iod_size_t *num,
	       iod_event_t *event);

/**
 * Get a list of KV pairs from one KV object.
 * Caller needs to provide and free the buffer for KV-pairs.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param offset [IN]	offset of kv pair inside the KV object(begin from 0),
 *			ordered by ascending alphabetical order of key (from
 *			'a' to 'z').
 * \param num [IN]	number of KV pairs
 * \param kvs [IN/OUT]	pointer to KV parameters packet list
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_kv_get_list(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		iod_off_t offset, iod_size_t num, iod_kv_params_t *kvs,
		iod_event_t *event);

/**
 * Retrieve a list of keys in the KV object.
 * Caller needs to provide and free the buffer for key list, every key is a
 * buffer of IOD_KV_KEY_MAXLEN length. Needs not malloc buffer for value
 * inside iod_kv_params_t, it will be ignored by this call.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param offset [IN]	offset of key inside the KV object(begin from 0),
 *			ordered by ascending alphabetical order (from 'a'
 *			to 'z').
 * \param num [IN]	number of KV pairs
 * \param kvs [IN/OUT]	pointer to KV parameters packet list
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_kv_list_key(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		iod_off_t offset, iod_size_t num, iod_kv_params_t *kvs,
		iod_event_t *event);

/**
 * Get the value belong to the key from one KV object.
 * Caller needs to provide and free the buffer for value, if user passed in
 * buffer length \a len is not enough to hold the value then when this routine
 * returns the \a len is set as the actual length of value.
 *
 * \param oh [IN]		object handle
 * \param tid [IN]		transaction ID
 * \param key [IN]		passed in key
 * \param value [IN/OUT]	returned value
 * \param len [IN/OUT]		pointer of length of passed in buffer of value
 * \param event [IN]		pointer to completion event
 *
 * \return			zero on success, negative value if error
 */
iod_ret_t
iod_kv_get_value(iod_handle_t oh, iod_trans_id_t tid, const char *key,
		 char *value, iod_size_t *len, iod_checksum_t *cs,
		 iod_event_t *event);

/**
 * Unlink a list of keys in the container.
 * Caller needs to provide and free the buffer for key list, every key is a
 * NULL termed string(within length of IOD_KV_KEY_MAXLEN). Needs not malloc
 * buffer for value inside iod_kv_params_t, it will be ignored by this call.
 *
 * \param oh [IN]	object handle
 * \param tid [IN]	transaction ID
 * \param hints[IN]	pointer to hints and can be NULL when no hint
 * \param num [IN]	number of KV pairs
 * \param kvs [IN/OUT]	pointer to KV parameters packet list
 * \param event [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_kv_unlink_keys(iod_handle_t oh, iod_trans_id_t tid, iod_hint_list_t *hints,
		   iod_size_t num, iod_kv_params_t *kvs, iod_event_t *event);


/* SECTION 5 ********** EVENT FUNCTIONS *************************/

/**
 * create an Event Queue
 *
 * \param eq [OUT]	returned EQ handle
 *
 * \return		zero on success, negative value if error
 */
int
iod_eq_create(iod_handle_t *eqh);

/**
 * Destroy an Event Queue.
 *
 * \param eqh [IN]	EQ to destroy
 * \param ev [IN]	pointer to completion event
 *
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_eq_destroy(iod_handle_t eqh, iod_event_t *ev);

/**
 * Retrieve completion events from an EQ
 *
 * \param eqh [IN]	EQ handle
 * \param wait_if [IN]	wait only if there's inflight event
 * \param timeout [IN]	how long is caller going to wait (micro-second)
 *			if \a timeout > 0,
 *			it can also be IOD_EQ_NOWAIT, IOD_EQ_WAIT.
 * \param n_events [IN]	size of \a events array, returned number of events
 *			should always be less than or equal to \a num
 * \param events [OUT]	pointer to returned events array
 *
 * \return		>= 0	returned number of events
 *			< 0	negative value if error
 */
iod_ret_t
iod_eq_poll(iod_handle_t eqh, int wait_if, uint64_t timeout, int n_events,
	    iod_event_t **events);


/**
 * Try to abort operations associated with this event.
 *
 * \param ev [IN]	event to abort
 * \return		zero on success, negative value if error
 */
iod_ret_t
iod_event_abort(iod_event_t *ev);

/**
 * Query how many outstanding events in EQ, if \a events is not NULL,
 * these events will be stored into it.
 * Events returned by query are still owned by IOD, it's not allowed to
 * finalize or free events returned by this function, but it's allowed
 * to call iod_event_abort() to abort inflight operation.
 * Also, status of returned event could be still in changing, for example,
 * returned "inflight" event can be turned to "completed" before acessing.
 * It's user's responsibility to guarantee that returned events would be
 * freed by polling process.
 *
 * \param eqh [IN]	EQ handle
 * \param query [IN]	query mode
 * \param n_events [IN]	size of \a events array
 * \param events [OUT]	pointer to returned events array
 * \return		>= 0	returned number of events
 *			 < 0	negative value if error
 */
int
iod_eq_query(iod_handle_t eqh, iod_ev_query_t query, unsigned int n_events,
	     iod_event_t **events);

/**
 * Initialize a new event for \a eq
 *
 * \param ev [IN]	event to initialize
 * \param eqh [IN]	where the event to be queued on
 *
 * \return		zero on success, negative value if error
 */
int
iod_event_init(iod_event_t *ev, iod_handle_t eqh);

/**
 * Finalize an event. If event has been passed into any IOD API, it can only
 * be finalized when it's been polled out from EQ, even it's aborted by
 * calling iod_event_abort().
 *
 * \param ev [IN]	event to finialize
 */
void
iod_event_fini(iod_event_t *ev);

/* TODO: details of kinds of hints */

#endif
