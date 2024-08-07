#ifndef __PROTOCOL__
#define __PROTOCOL__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>
#include "frame.h"
#include "timeop.h"

#define GO_BACK_WINDOW_SIZE 5
#define RAW_TIMEOUT 20 // Tempo em ms
#define BASE_TIMEOUT 200
#define MAX_TIMEOUT 10000 // 10 segundos

#define min(a, b) a > b ? b : a
#define max(a, b) a > b ? a : b

#define TIMEOUT_RECOVER_METHOD decreaeTimeout

// Flags recvFrame (OR)
#define RECV_NOTIMEOUT_INCREASE 0x1

// Estrutura de descritor de arquivo
struct fdesc {
	off_t size;
	struct utimbuf t;
};

void printError();

int newSocket(char *device);

int sendFrame(int sock, Frame *f, int flags);

int recvFrame(int sock, Frame *f, int flags);

int sendFrameUntilACK(int sock, Frame *f, int flags);

int sendFile(int sock, char *fname);

size_t readDataFrame(Frame *f, uint8_t *dst, size_t dstsize, size_t dstused);

int repeatLastACK(int sock, int flags);

int getTimeoutNumber();

void resetTimeout();

#endif
