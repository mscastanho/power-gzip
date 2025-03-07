#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <endian.h>
#include "test.h"
#include "test_utils.h"

char ran_data[DATA_MAX_LEN];

static char dict[] = {
	'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u',
	'v', 'w', 'x', 'y', 'z',
	',', '.', '!', '?', '.', '{', '}'
};

void generate_all_data(int len, char digit)
{
	assert(len > 0);

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		ran_data[i] = digit;
	}
}

void generate_random_data(int len)
{
	assert(len > 0);

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		ran_data[i] = dict[rand() % sizeof(dict)];
	}
}

char* generate_allocated_random_data(unsigned int len)
{
	assert(len > 0);

	char *data = malloc(len);
	if (data == NULL) return NULL;

	srand(time(NULL));

	for (int i = 0; i < len; i++) {
		data[i] = dict[rand() % sizeof(dict)];
	}
	return data;
}

int compare_data(char* src, char* dest, int len)
{
	for (int i = 0; i < len; i++) {
		if (src[i] != dest[i]) {
			printf(" src[%d] %02x != dest[%d] %02x \n", i, src[i], i, dest[i]);
			return TEST_ERROR;
		}
	}
	return TEST_OK;
}

/* TODO: mark these as static later. */
alloc_func zalloc = (alloc_func) 0;
free_func zfree = (free_func) 0;

/* Use nx to deflate. */
int _test_nx_deflate(Byte* src, unsigned int src_len, Byte* compr,
		     unsigned int compr_len, int step)
{
	int err;
	z_stream c_stream;

	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	err = nx_deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	if (err != 0) {
		printf("nx_deflateInit err %d\n", err);
		return TEST_ERROR;
	}

	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	while (c_stream.total_in != src_len && c_stream.total_out < compr_len) {
		step = (step < (src_len - c_stream.total_in))
		       ? (step) : (src_len - c_stream.total_in);
		c_stream.avail_in = c_stream.avail_out = step;
		err = nx_deflate(&c_stream, Z_NO_FLUSH);
		if (c_stream.total_in > src_len) break;
		if (err < 0) {
			printf("*** failed: nx_deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	assert(c_stream.total_in == src_len);

	for (;;) {
		c_stream.avail_out = 1;
		err = nx_deflate(&c_stream, Z_FINISH);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: nx_deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}

	err = nx_deflateEnd(&c_stream);
	if (err != 0)
		return TEST_ERROR;

	return TEST_OK;
}

/* Use zlib to deflate. */
int _test_deflate(Byte* src, unsigned int src_len, Byte* compr,
		  unsigned int compr_len, int step)
{
	int err;
	z_stream c_stream;

	c_stream.zalloc = zalloc;
	c_stream.zfree = zfree;
	c_stream.opaque = (voidpf)0;

	err = deflateInit(&c_stream, Z_DEFAULT_COMPRESSION);
	if (err != 0) {
		printf("deflateInit err %d\n", err);
		return TEST_ERROR;
	}

	c_stream.next_in  = (z_const unsigned char *)src;
	c_stream.next_out = compr;

	while (c_stream.total_in != src_len && c_stream.total_out < compr_len) {
		c_stream.avail_in = c_stream.avail_out = step;
		err = deflate(&c_stream, Z_NO_FLUSH);
		if (c_stream.total_in > src_len) break;
		if (err < 0) {
			printf("*** failed: deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	assert(c_stream.total_in == src_len);

	for (;;) {
		c_stream.avail_out = 1;
		err = deflate(&c_stream, Z_FINISH);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: deflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	printf("\n*** c_stream.total_out %ld\n",
	       (unsigned long) c_stream.total_out);

	err = deflateEnd(&c_stream);
	if (err != 0)
		return TEST_ERROR;

	return TEST_OK;
}

/* Use zlib to inflate. */
int _test_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr,
		  unsigned int uncomprLen, Byte* src, unsigned int src_len,
		  int step)
{
	int err;
	z_stream d_stream;

	memset(uncompr, 0, uncomprLen);

	d_stream.zalloc = zalloc;
	d_stream.zfree = zfree;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;

	err = inflateInit(&d_stream);
	while (d_stream.total_out < uncomprLen
	       && d_stream.total_in < comprLen) {
		d_stream.avail_in = d_stream.avail_out = step;
		err = inflate(&d_stream, Z_NO_FLUSH);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: inflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	printf("*** d_stream.total_in %ld d_stream.total_out %ld src_len %d\n",
	       (unsigned long) d_stream.total_in,
	       (unsigned long) d_stream.total_out, src_len);
	assert(d_stream.total_out == src_len);

	err = inflateEnd(&d_stream);

	if (compare_data((char *) uncompr, (char *) src, src_len))
		return TEST_ERROR;

	return TEST_OK;
}

/* Use nx to inflate. */
int _test_nx_inflate(Byte* compr, unsigned int comprLen, Byte* uncompr,
		     unsigned int uncomprLen, Byte* src, unsigned int src_len,
		     int step, int flush)
{
	int err;
	z_stream d_stream;

	memset(uncompr, 0, uncomprLen);

	d_stream.zalloc = zalloc;
	d_stream.zfree = zfree;
	d_stream.opaque = (voidpf)0;

	d_stream.next_in  = compr;
	d_stream.avail_in = 0;
	d_stream.next_out = uncompr;

	err = nx_inflateInit(&d_stream);
	while (d_stream.total_out < uncomprLen
	       && d_stream.total_in < comprLen) {
		d_stream.avail_in = d_stream.avail_out = step;
		err = nx_inflate(&d_stream, flush);
		if (err == Z_STREAM_END) break;
		if (err < 0) {
			printf("*** failed: nx_inflate returned %d\n", err);
			return TEST_ERROR;
		}
	}
	printf("*** d_stream.total_in %ld d_stream.total_out %ld src_len %d\n",
	       (unsigned long) d_stream.total_in,
	       (unsigned long) d_stream.total_out, src_len);
	assert(d_stream.total_out == src_len);

	err = nx_inflateEnd(&d_stream);

	if (compare_data((char *) uncompr, (char *) src, src_len))
		return TEST_ERROR;

	return TEST_OK;
}
