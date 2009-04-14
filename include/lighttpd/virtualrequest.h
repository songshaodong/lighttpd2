#ifndef _LIGHTTPD_VIRTUALREQUEST_H_
#define _LIGHTTPD_VIRTUALREQUEST_H_

#ifndef _LIGHTTPD_BASE_H_
#error Please include <lighttpd/base.h> instead of this file
#endif

typedef enum {
	/* waiting for request headers */
	VRS_CLEAN,

	/* all headers received, now handling them, set up input filters
	 *   this state is set by the previous vrequest after VRS_WROTE_RESPONSE_HEADERS (or the main connection),
	 *   and the handle_request function is called (which execute the action stack by default)
	 */
	VRS_HANDLE_REQUEST_HEADERS,

	/* request headers handled, input filters ready; now content is accepted
	 *   this state is set via handle_indirect (handle_direct skips to VRS_HANDLE_RESPONSE_HEADERS
	 */
	VRS_READ_CONTENT,

	/* all response headers written, now set up output filters */
	VRS_HANDLE_RESPONSE_HEADERS,

	/* output filters ready, content can be written */
	VRS_WRITE_CONTENT,

	/* request done */
/* 	VRS_END, */

	VRS_ERROR
} vrequest_state;

typedef handler_t (*filter_handler)(vrequest *vr, filter *f);
typedef void (*filter_free)(vrequest *vr, filter *f);
typedef handler_t (*vrequest_handler)(vrequest *vr);
typedef handler_t (*vrequest_plugin_handler)(vrequest *vr, plugin *p);

struct filter {
	chunkqueue *in, *out;
	filter_handler handle_data;
	filter_free handle_free;
	gpointer param;
};

struct filters {
	GPtrArray *queue;
	chunkqueue *in, *out;
	guint skip_ndx;
};

struct vrequest_ref {
	guint refcount;
	vrequest *vr; /* This is only accesible by the worker thread the vrequest belongs to, and it may be NULL if the vrequest is already reset */
};

struct vrequest {
	connection *con;
	vrequest_ref *ref;

	option_value *options;

	vrequest_state state;

	vrequest_handler
		handle_request_headers,
		handle_response_headers, handle_response_body,
		handle_response_error; /* this is _not_ for 500 - internal error */

	GPtrArray *plugin_ctx;
	plugin *backend;

	request request;
	physical physical;
	response response;

	/* environment entries will be passed to the backends */
	environment env;

	/* -> vr_in -> filters_in -> in -> handle -> out -> filters_out -> vr_out -> */
	gboolean cq_memory_limit_hit; /* stop feeding chunkqueues with memory chunks */
	filters filters_in, filters_out;
	chunkqueue *vr_in, *vr_out;
	chunkqueue *in, *out;

	action_stack action_stack;
	gboolean actions_wait_for_response;

	gint queued;
	GList job_queue_link;

	GPtrArray *stat_cache_entries;
};

#define VREQUEST_WAIT_FOR_RESPONSE_HEADERS(vr) \
	do { \
		if (vr->state == VRS_HANDLE_REQUEST_HEADERS) { \
			VR_ERROR(vr, "%s", "Cannot wait for response headers as no backend handler found - fix your config"); \
			return HANDLER_ERROR; \
		} else if (vr->state < VRS_HANDLE_RESPONSE_HEADERS) { \
			return HANDLER_WAIT_FOR_EVENT; \
		} \
	} while (0)

LI_API vrequest* vrequest_new(struct connection *con, vrequest_handler handle_response_headers, vrequest_handler handle_response_body, vrequest_handler handle_response_error, vrequest_handler handle_request_headers);
LI_API void vrequest_free(vrequest *vr);
LI_API void vrequest_reset(vrequest *vr);

LI_API vrequest_ref* vrequest_acquire_ref(vrequest *vr);
LI_API vrequest* vrequest_release_ref(vrequest_ref *vr_ref);

LI_API void vrequest_add_filter_in(vrequest *vr, filter_handler handle_data, filter_free handle_free, gpointer param);
LI_API void vrequest_add_filter_out(vrequest *vr, filter_handler handle_data, filter_free handle_free, gpointer param);

/* Signals an internal error; handles the error in the _next_ loop */
LI_API void vrequest_error(vrequest *vr);

LI_API void vrequest_backend_overloaded(vrequest *vr);
LI_API void vrequest_backend_dead(vrequest *vr);
LI_API void vrequest_backend_error(vrequest *vr, backend_error berror);

/* received all request headers */
LI_API void vrequest_handle_request_headers(vrequest *vr);
/* received (partial) request content */
LI_API void vrequest_handle_request_body(vrequest *vr);
/* received all response headers/status code - call once from your indirect handler */
LI_API void vrequest_handle_response_headers(vrequest *vr);
/* received (partial) response content - call from your indirect handler */
LI_API void vrequest_handle_response_body(vrequest *vr);

/* response completely ready */
LI_API gboolean vrequest_handle_direct(vrequest *vr);
/* handle request over time */
LI_API gboolean vrequest_handle_indirect(vrequest *vr, plugin *p);
LI_API gboolean vrequest_is_handled(vrequest *vr);

LI_API void vrequest_state_machine(vrequest *vr);
LI_API void vrequest_joblist_append(vrequest *vr);
LI_API void vrequest_joblist_append_async(vrequest *vr);

LI_API gboolean vrequest_stat(vrequest *vr);

LI_API gboolean vrequest_redirect(vrequest *vr, GString *uri);

#endif
