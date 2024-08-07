#include "frame.h"

struct sequencenumber seqnumber = {.framesequence = 0};

// uint8_t frameseq = 0;

// Aloca memória para um vetor de @framenum Frames e inicializa o marcador
// de início de cada um.
// Retorna NULL em caso de erro.
Frame *newFrames(int framenum) {
	Frame *fs = calloc(framenum, sizeof(Frame));
	if (!fs)
		return NULL;
	for (int i = 0; i < framenum; i++) {
		fs[i].startMarker = START_MARKER;
	}

	return fs;
}

void initFrame(Frame *f) {
	f->startMarker = START_MARKER;
	f->sequence = 0;
	f->type = 0;
	f->dataSize = 0;
	memset(f->data, 0, sizeof(f->data)+1);
}

// Recebe o endereço de um frame e atribui os valores dos atributos, deixando
// o frame pronto para o envio.
// OBS: Se o frame não for enviado o protocolo irá parar de funcionar porque a
// sequência será quebrada.
void mountFrame(Frame *f, enum FrameType type, uint8_t dataSize, uint8_t *data) {
	f->dataSize = dataSize;
	f->sequence = frameseq++;
	f->type = type;
	memcpy(f->data, data, dataSize);
	setFrameCRC(f);
}

void mountACK(Frame *f, uint8_t sequence) {
	f->sequence = sequence;
	f->type = ACK;
	f->dataSize = 1;
	memset(f->data, 0xFF, 1);
	setFrameCRC(f);
}

void mountNACK(Frame *f, uint8_t sequence) {
	f->sequence = sequence;
	f->type = NACK;
	f->dataSize = 1;
	memset(f->data, 0xFF, 1);
	setFrameCRC(f);
}

uint8_t getFrameCRC(Frame *f) {
	return f->data[f->dataSize];
}

void setFrameCRC(Frame *f) {
	f->data[f->dataSize] = calculateCRC(f);
}

uint8_t calculateCRC(Frame *f) {
    uint8_t len  = f->dataSize+1; // Tamanho total do frame = tamanho de dados + tamanho do header sem o preâmbulo (1)
    uint8_t crc = 0xff;
	uint8_t *buffer = ((uint8_t *) f) + 1; // Pula o marcador de inicio
    for (int i = 0; i < len; i++) {
        crc ^= buffer[i];
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0)
                crc = (crc << 1) ^ 0x49;
            else
                crc <<= 1;
        }
    }

    return crc;
}

uint8_t checkCRC (Frame *f) {
    return calculateCRC(f) == getFrameCRC(f);
}

size_t getFrameTotalSize(Frame *f) {
	size_t framesize = 3 + f->dataSize; // 2 (header) + 1 (CRC-8) + dsize
	if (framesize < FRAME_MIN_TOTAL_SIZE)
		return FRAME_MIN_TOTAL_SIZE;
	return framesize;
}

uint8_t getError() {
	switch (errno) {
	case EACCES:
		return NOACCESS;
		break;
	case ENOENT:
		return NOTFOUND;
		break;
	case ENOSPC:
		return DISKFULL;
		break;
	default:
		fprintf(stderr, "Unexpected error occured.\n");
		return UNEXPECTED;
		break;
	}
}

void printErrorFrame(Frame *f) {
	uint8_t errorcode = ((uint8_t *) f->data)[0];
	FILE *out = stderr;
	switch (errorcode) {
	case NOERROR:
		fprintf(out, "No error.\n");
		break;
	case NOACCESS:
		fprintf(out, "Can't access file.\n");
		break;
	case NOTFOUND:
		fprintf(out, "File not found.\n");
		break;
	case DISKFULL:
		fprintf(out, "Disk is full.\n");
		break;
	case UNEXPECTED:
		fprintf(out, "Unexpected error.\n");
		break;
	default:
		fprintf(out, "Unknown error.\n");
		break;
	}
}

Frame *newDataFrameBuffer(uint8_t bufsize) {
	Frame *f = newFrames(bufsize);
	if (!f)
		return NULL;
	for (int i = 0; i < bufsize; i++) {
		f[i].type = DATA;
	}
	return f;
}

char *frameTypeString(enum FrameType t) {
	switch (t) {
	case ACK:
		return "ACK";
	case NACK:
		return "NACK";
	case LIST:
		return "LIST";
	case DOWNLOAD:
		return "DOWNLOAD";
	case SHOW:
		return "SHOW";
	case FDESC:
		return "FDESC";
	case DATA:
		return "DATA";
	case ENDTX:
		return "ENDTX";
	case ERROR:
		return "ERROR";
	case TIMEOUT:
		return "TIMEOUT TYPE?";
	default:
		return "UNKNOWN";
	}
}

void printFrame(Frame *f) {
	char *type = frameTypeString(f->type);
	printf("Sequence: %d\n", f->sequence);
	printf("Type: %s", type);
	if (!(strcmp(type, "UNKNOWN")))
		printf(" (%u)", f->type);
	printf("\nData Size: %u bytes\n", f->dataSize);
	printf("Data:");
	for (int i = 0; i <= f->dataSize; i++) {
		printf(" %02x", f->data[i]);
	}
	printf("\nCRC: %02x\n", f->data[f->dataSize]);
	printf("CRC calculado: %02x\n", calculateCRC(f));
}

uint8_t incrementFrameSeq(int i) {
	return frameseq += i;
}
