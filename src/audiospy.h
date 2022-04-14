/** audiospy: shared data
2022, Simon Zolin */

struct aus_hello {
	char version;
	char opcode;

	char format;
	char sample_rate[4];
	char channels;
};
