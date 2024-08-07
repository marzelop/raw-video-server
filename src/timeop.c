#include "timeop.h"

struct timeval timestamp() {
	struct timeval t;
	gettimeofday(&t, NULL);

	return t;
}

struct timeval timevalAdd (struct timeval *t1, struct timeval *t2) {
	struct timeval result = {
		.tv_sec = t1->tv_sec + t2->tv_sec,
		.tv_usec = t1->tv_usec + t2->tv_usec
	};
	// Converte o excesso de usec em segundos
	result.tv_sec += result.tv_usec / 1000000;
	result.tv_usec = result.tv_usec % 1000000;

	return result;
}

int verifyTimeout(struct timeval limit) {
	struct timeval now;
	gettimeofday(&now, NULL);
	return timecmp(&now, &limit, >);
}

