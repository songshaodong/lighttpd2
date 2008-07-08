#include <stdio.h>


#include "base.h"
#include "log.h"

#include "http_request_parser.h"

int request_test() {
	chunkqueue *cq;
	request *req;
	http_request_ctx ctx;
	handler_t res;

	cq = chunkqueue_new();
	req = request_new();

	http_request_parser_init(&ctx, req, cq);

	chunkqueue_append_mem(cq, CONST_STR_LEN(
		"GET / HTTP/1.1\r\n"
		"\r\n"
	));

	res = http_request_parse(NULL, NULL, &ctx);
	if (res != HANDLER_GO_ON) {
		ERROR("Parser return %i", res);
		return -1;
	}

	return 0;
}

int main() {
	GTimeVal start, end;
	gboolean result;

	/* config parser test */
	config_parser_init();
	g_get_current_time(&start);
	result = config_parser_file("/home/icy/dev/c/lighttpd/test.conf");
	g_get_current_time(&end);

	printf("parsed config in %ld seconds %ld milliseconds and %ld microseconds\n",
		end.tv_sec - start.tv_sec,
		(end.tv_usec - start.tv_usec) / 1000,
		(end.tv_usec - start.tv_usec) % 1000
	);

	return request_test();
}