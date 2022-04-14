/** audiospy: client
2022, Simon Zolin */

#include <audiospy.h>
#include <FFOS/std.h>
#include <FFOS/socket.h>
#include <FFOS/error.h>
#include <ffaudio/audio.h>
#include <FFOS/ffos-extern.h>

struct cl_conf {
	char ip[4];
	ffuint port;
	ffuint buf_size;
};

struct cl_ctx {
	const ffaudio_interface *aif;
	struct cl_conf conf;
};
struct cl_ctx *cl;

void cl_destroy()
{
	cl->aif->uninit();
}

#define cl_log(fmt, ...) \
do { \
	ffstdout_fmt(fmt "\n", ##__VA_ARGS__); \
} while (0)

/** Parse IPv4 address.
Return 0 if the whole input is parsed;
  >0 number of processed bytes;
  <0 on error */
static inline int ffip4_parse(char *ip4, const char *s, ffsize len)
{
	ffuint nadr = 0, ndig = 0, b = 0, i;

	for (i = 0;  i != len;  i++) {
		int ch = s[i];

		if ((ch >= '0' && ch <= '9') && ndig != 3) {
			b = b * 10 + (ch) - '0';
			if (b > 255)
				return -1; // "256."
			ndig++;

		} else if (ndig == 0) {
			return -1; // "1.?"

		} else if (nadr == 3) {
			ip4[nadr] = b;
			return i;

		} else if (ch == '.') {
			ip4[nadr++] = b;
			b = 0;
			ndig = 0;

		} else {
			return -1;
		}
	}

	if (nadr == 3 && ndig != 0) {
		ip4[nadr] = b;
		return 0;
	}

	return -1;
}


/** Read configuration */
int conf_read(struct cl_conf *conf, int argc, const char **argv)
{
	if (argc < 3) {
		cl_log("Usage: audiospy_cl IP PORT");
		return -1;
	}

	conf->buf_size = 64*1024;

	if (0 != ffip4_parse(conf->ip, argv[1], ffsz_len(argv[1]))) {
		cl_log("bad IP address");
		return -1;
	}

	ffstr sport = FFSTR_INITZ(argv[2]);
	if (!ffstr_to_uint32(&sport, &conf->port)
		|| conf->port == 0 || conf->port > 0xffff) {
		cl_log("bad port number");
		return -1;
	}

	return 0;
}

/** Parse Hello message from server */
int hello_parse(ffstr data, ffaudio_conf *aconf)
{
	const struct aus_hello *hello = (void*)data.ptr;
	if (!(hello->version == 1 && hello->opcode == 1)) {
		cl_log("bad Hello from server");
		return -1;
	}

	aconf->format = hello->format;
	aconf->sample_rate = ffint_be_cpu32_ptr(hello->sample_rate);
	aconf->channels = hello->channels;
	return 0;
}

/** Show progress spinner */
void show_progress()
{
	static const char progress[] = "|/-|/-\\";
	static int iprog;
	char s[] = { '\r', progress[iprog] };
	iprog = (iprog+1) % 7;
	ffstdout_write(s, 2);
}

int main(int argc, const char **argv)
{
	ffaudio_buf *abuf = NULL;
	ffstr buf = {};
	ffsock sk = FFSOCK_NULL;
	int r;

	cl = ffmem_new(struct cl_ctx);
	if (NULL == (cl->aif = ffaudio_default_interface())) {
		cl_log("error: ffaudio_default_interface()");
		goto end;
	}

	if (0 != conf_read(&cl->conf, argc, argv))
		goto end;

	ffsock_init(FFSOCK_INIT_SIGPIPE | FFSOCK_INIT_WSA);
	if (FFSOCK_NULL == (sk = ffsock_create_tcp(AF_INET, 0))) {
		cl_log("error: ffsock_create_tcp(): %E", fferr_last());
		goto end;
	}

	ffsockaddr a = {};
	ffsockaddr_set_ipv4(&a, cl->conf.ip, cl->conf.port);
	if (0 != ffsock_connect(sk, &a)) {
		cl_log("error: ffsock_connect(): %E", fferr_last());
		goto end;
	}

	ffaudio_init_conf aiconf = {};
	if (0 != cl->aif->init(&aiconf)) {
		cl_log("error: cl->aif->init()");
		goto end;
	}

	ffstr_alloc(&buf, cl->conf.buf_size);

	while (buf.len < sizeof(struct aus_hello)) {
		if (0 >= (r = ffsock_recv(sk, buf.ptr + buf.len, sizeof(struct aus_hello), 0))) {
			cl_log("error: ffsock_recv(): %E", fferr_last());
			goto end;
		}
		buf.len += r;
	}

	ffaudio_conf aconf = {};
	if (0 != hello_parse(buf, &aconf))
		goto end;

	abuf = cl->aif->alloc();
	if (0 != cl->aif->open(abuf, &aconf, FFAUDIO_PLAYBACK)) {
		cl_log("error: audio device: %s", cl->aif->error(abuf));
		goto end;
	}
	cl_log("opened audio device: %u/%u/%u", aconf.format, aconf.sample_rate, aconf.channels);

	for (;;) {
		if (0 >= (r = ffsock_recv(sk, buf.ptr, cl->conf.buf_size, 0))) {
			if (r == 0) {
				cl_log("server closed the connection");
				goto end;
			}
			cl_log("error: ffsock_recv(): %E", fferr_last());
			goto end;
		}
		buf.len = r;

		show_progress();

		ffstr data = buf;
		while (data.len != 0) {
			if (0 > (r = cl->aif->write(abuf, data.ptr, data.len))) {
				cl_log("error: cl->aif->write(): %s", cl->aif->error(abuf));
				goto end;
			}
			ffstr_shift(&data, r);
		}
	}

end:
	ffsock_close(sk);
	ffstr_free(&buf);
	cl->aif->free(abuf);
	cl_destroy();
	ffmem_free(cl);
	return 0;
}
