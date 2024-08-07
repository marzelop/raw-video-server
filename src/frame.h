#ifndef __FRAME_H__
#define __FRAME_H__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define START_MARKER 0x7E
#define FRAME_MAX_DATA 63
#define FRAME_SEQ_BITS 5
#define FRAME_SEQ_MAX  (0x1 << 5)
#define FRAME_SEQ_MASK (FRAME_SEQ_MAX-1)

#define FRAME_MIN_TOTAL_SIZE 67 // sizeof(Frame) == 67

struct sequencenumber {
	uint8_t framesequence:FRAME_SEQ_BITS;
};

#define frameseq seqnumber.framesequence
#define frameseqaddr &seqnumber

extern struct sequencenumber seqnumber;

enum FrameType {
	ACK=0,
	NACK=1,
	TIMEOUT=8,
	LIST=10,
	DOWNLOAD=11,
	SHOW=16,
	FDESC=17,
	DATA=18,
	ENDTX=30,
	ERROR=31
};

enum ErrorType {
	NOERROR=0,
	NOACCESS,
	NOTFOUND,
	DISKFULL,
	UNEXPECTED
};

// Pragma para evitar bytes de padding na struct
#pragma pack(push,1)
typedef struct frame {
	uint8_t startMarker;
	uint8_t dataSize:6;
	uint8_t sequence:FRAME_SEQ_BITS;
	uint8_t type:5;
	uint8_t data[FRAME_MAX_DATA+1]; // Dados + CRC-8 (primeiro byte após o fim dos dados)
} Frame;
#pragma pack(pop)

// Aloca memória para um vetor de @framenum Frames e inicializa o marcador
// de início de cada um.
// Retorna NULL em caso de erro.
Frame *newFrames(int framenum);

// Inicializa um frame
void initFrame(Frame *f);

// Recebe o endereço de um frame e atribui os valores dos atributos, deixando
// o frame pronto para o envio.
void mountFrame(Frame *f, enum FrameType type, uint8_t dataSize, uint8_t *data);

void mountACK(Frame *f, uint8_t sequence);

void mountNACK(Frame *f, uint8_t sequence);

// Retorna o valor do CRC de um frame (não calcula)
uint8_t getFrameCRC(Frame *f);

// Calcula e atribui o valor do CRC de um frame
void setFrameCRC(Frame *f);

// Calcula o valor do CRC de um frame
uint8_t calculateCRC (Frame *f);

// Verifica se o CRC do frame está correto ou há algum erro
// Retorna 1 caso não tenha erro, e 0 caso contrário
uint8_t checkCRC (Frame *f);

// Retorna o código de erro da última operação, baseado no errno
// Retorna UNEXPECTED em caso de erro não esperado dentro do protocolo
uint8_t getError();

void printErrorFrame(Frame *f);

Frame *newDataFrameBuffer(uint8_t bufsize);

void printFrame(Frame *f);

char *frameTypeString(enum FrameType t);

uint8_t incrementFrameSeq(int i);

size_t getFrameTotalSize(Frame *f);

#endif
