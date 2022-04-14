/** audiospy: server
2022, Simon Zolin */

#include <audiospy.h>
#include <FFOS/std.h>
#include <FFOS/socket.h>
#include <FFOS/error.h>
#include <ffaudio/audio.h>
#include <FFOS/ffos-extern.h>

struct sv_conf {
	ffuint port;
	ffuint silent;
	ffuint aformat;
	ffuint sample_rate;
	ffuint channels;
};

struct sv_ctx {
	const ffaudio_interface *aif;
	struct sv_conf conf;
	ffsock lsk;
};
struct sv_ctx *sv;

void sv_destroy()
{
	ffsock_close(sv->lsk);
	sv->aif->uninit();
}

#define sv_log(fmt, ...) \
do { \
	if (!sv->conf.silent) \
		ffstdout_fmt(fmt "\n", ##__VA_ARGS__); \
} while (0)

/** Read configuration */
int conf_read(struct sv_conf *conf, int argc, const char **argv)
{
	if (argc < 2) {
		sv_log("Usage: audiospy_sv PORT");
		return -1;
	}

	// set default format
	conf->aformat = FFAUDIO_F_INT16;
	conf->sample_rate = 48000;
	conf->channels = 2;

	ffstr sport = FFSTR_INITZ(argv[1]);
	if (!ffstr_to_uint32(&sport, &conf->port)
		|| conf->port == 0 || conf->port > 0xffff) {
		sv_log("bad port number");
		return -1;
	}

	return 0;
}

/** Return listening socket or FFSOCK_NULL on error */
ffsock lsock_prepare()
{
	ffsock lsk;
	ffsock_init(FFSOCK_INIT_SIGPIPE | FFSOCK_INIT_WSA);
	if (FFSOCK_NULL == (lsk = ffsock_create_tcp(AF_INET, 0))) {
		sv_log("error: ffsock_create_tcp(): %E", fferr_last());
		goto end;
	}

	ffsockaddr a = {};
	ffsockaddr_set_ipv4(&a, NULL, sv->conf.port);
	if (0 != ffsock_bind(lsk, &a)) {
		sv_log("error: ffsock_bind(): %E", fferr_last());
		goto end;
	}
	if (0 != ffsock_listen(lsk, SOMAXCONN)) {
		sv_log("error: ffsock_listen(): %E", fferr_last());
		goto end;
	}

	return lsk;

end:
	ffsock_close(lsk);
	return FFSOCK_NULL;
}

int aud_open(ffaudio_buf *abuf, ffaudio_conf *aconf)
{
	aconf->format = sv->conf.aformat;
	aconf->sample_rate = sv->conf.sample_rate;
	aconf->channels = sv->conf.channels;
	int r = sv->aif->open(abuf, aconf, FFAUDIO_CAPTURE);
	if (r == FFAUDIO_EFORMAT)
		r = sv->aif->open(abuf, aconf, FFAUDIO_CAPTURE);
	if (r != 0) {
		sv_log("error: audio device open: %s", sv->aif->error(abuf));
		return -1;
	}
	sv_log("opened audio device: %u/%u/%u", aconf->format, aconf->sample_rate, aconf->channels);
	return 0;
}

/** Show progress spinner */
void show_progress()
{
	static const char progress[] = "|/-|/-\\";
	static int iprog;
	char s[] = { '\r', progress[iprog] };
	iprog = (iprog+1)%7;
	ffstdout_write(s, 2);
}

/** Send data to client */
int data_send(ffsock sk, ffstr data)
{
	while (data.len != 0) {
		int r;
		if (0 > (r = ffsock_send(sk, data.ptr, data.len, 0))) {
			sv_log("warning: ffsock_send(): %E", fferr_last());
			return -1;
		}
		ffstr_shift(&data, r);
	}
	return 0;
}

/** Send Hello message to client */
int hello_send(ffsock sk, ffaudio_conf *aconf)
{
	struct aus_hello hello = {
		.version = 1,
		.opcode = 1,
		.format = aconf->format,
		.channels = aconf->channels,
	};
	*(ffuint*)&hello.sample_rate = ffint_be_cpu32(aconf->sample_rate);
	ffstr data = FFSTR_INITN(&hello, sizeof(hello));
	return data_send(sk, data);
}

int main(int argc, const char **argv)
{
	ffaudio_buf *abuf = NULL;
	int r;
	ffsock csk = FFSOCK_NULL;

	sv = ffmem_new(struct sv_ctx);
	sv->lsk = FFSOCK_NULL;
	if (NULL == (sv->aif = ffaudio_default_interface())) {
		sv_log("error: ffaudio_default_interface()");
		goto end;
	}

	if (0 != conf_read(&sv->conf, argc, argv))
		goto end;

	if (FFSOCK_NULL == (sv->lsk = lsock_prepare()))
		goto end;

	ffaudio_init_conf aiconf = {};
	if (0 != sv->aif->init(&aiconf)) {
		sv_log("error: sv->aif->init()");
		goto end;
	}

	for (;;) {
		if (csk == FFSOCK_NULL) {
			sv_log("waiting for client...");
			ffsockaddr peer = {};
			if (FFSOCK_NULL == (csk = ffsock_accept(sv->lsk, &peer, 0))) {
				sv_log("error: ffsock_accept(): %E", fferr_last());
				goto end;
			}
			sv_log("client connected");

			abuf = sv->aif->alloc();
			ffaudio_conf aconf = {};
			if (0 != aud_open(abuf, &aconf))
				goto end;

			if (0 != hello_send(csk, &aconf)) {
				ffsock_close(csk);  csk = FFSOCK_NULL;
				continue;
			}
		}

		ffstr data;
		if (0 > (r = sv->aif->read(abuf, (const void**)&data.ptr))) {
			sv_log("error: sv->aif->read(): %s", sv->aif->error(abuf));
			goto end;
		}
		data.len = r;

		if (!sv->conf.silent)
			show_progress();

		if (0 != data_send(csk, data)) {
			ffsock_close(csk);  csk = FFSOCK_NULL;
			sv->aif->free(abuf);  abuf = NULL;
		}
	}

end:
	ffsock_close(csk);
	sv->aif->free(abuf);
	sv_destroy();
	ffmem_free(sv);
	return 0;
}
