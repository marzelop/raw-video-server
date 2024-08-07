#include "protocol.h"

#define ESCAPE_BYTE 0xFF

uint8_t ethbuffer[ETH_FRAME_LEN];
struct timeval protocoltimeout = CREATE_TIMEVAL(BASE_TIMEOUT);
int timeoutnum = 0;
uint8_t lastACK = 0;
int logextra = 0;

void printError() {
	fprintf(stderr, "%s\n", strerror(errno));
}

int getTimeoutNumber() {
	return timeoutnum;
}

void increaseTimeout() {
	int newtimeout;
	timeoutnum++;
	if (timeoutnum >= 10)
		printf("Timeout num %d\n", timeoutnum);
	newtimeout = BASE_TIMEOUT << timeoutnum; // BASE_TIMEOUT * 2^(timeoutnum)
	if (newtimeout > MAX_TIMEOUT)
		newtimeout = MAX_TIMEOUT;

	protocoltimeout = CREATE_TIMEVAL(newtimeout);
}

void resetTimeout() {
	timeoutnum = 0;
	protocoltimeout = CREATE_TIMEVAL(BASE_TIMEOUT);
}

void decreaeTimeout() {
	int newtimeout;
	timeoutnum--;
	if (timeoutnum < 0)
		timeoutnum = 0;
	newtimeout = BASE_TIMEOUT << timeoutnum; // BASE_TIMEOUT * 2^(timeoutnum)
	protocoltimeout = CREATE_TIMEVAL(newtimeout);
}

int newSocket(char *device) {
	static struct timeval rawtimeout = CREATE_TIMEVAL(RAW_TIMEOUT);
	int sock;
	struct ifreq ir;
	struct sockaddr_ll addr;
	struct packet_mreq mr;

	if ((sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
		fprintf(stderr, "Error at socket creation.\n");
		printError();
		exit(1);
	}

	memset(&ir, 0, sizeof(struct ifreq));
	strcpy(ir.ifr_ifrn.ifrn_name, device); // Adiciona o nome da interface de rede
	// ioctl está pegando o índice da interface desejada e colocando em ir.ifr_index
	if (ioctl(sock, SIOCGIFINDEX, &ir) == -1) {
		printf("Error at getting interface index.\n");
		printError(); 
		exit(1);
	}

	memset(&addr, 0, sizeof(addr)); 	/*IP do dispositivo*/
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ALL);
	addr.sll_ifindex = ir.ifr_ifindex;
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		printf("Error at binding socket to interface.\n");
		printError();
		exit(1);
	}

	memset(&mr, 0, sizeof(mr));          /*Modo Promiscuo*/
	mr.mr_ifindex = ir.ifr_ifindex;
	mr.mr_type = PACKET_MR_PROMISC;
	if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1)	{
		printf("Error at setting socket options.\n");
		printError();
		exit(1);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rawtimeout, sizeof(rawtimeout)) == -1)	{
		printf("Error at setting socket raw timeout.\n");
		printError();
		exit(-1);
	}

	return sock;
}

int sendFrame(int sock, Frame *f, int flags) {
#ifdef _SEND_DELAY_
	usleep(random() % 220000);
#endif
	int retv = send(sock, f, getFrameTotalSize(f), flags);
#ifdef _LOG_FRAMES_
	if (f->type <= NACK)
		printf("ACK %d enviado\n", f->sequence);
	else {
		printf("Frame %d enviado\n", f->sequence);
	}
#endif
	if (retv < 0)
		printError();
	return retv;
}

int repeatLastACK(int sock, int flags) {
	ssize_t retv;
	Frame f;
	initFrame(&f);
	mountACK(&f, lastACK);
	retv = sendFrame(sock, &f, flags);
	if (retv < 0)
		printError();
	return retv;
}

int sendACK(int sock, Frame *f, uint8_t seq, int flags) {
	ssize_t retv;
	mountACK(f, seq);
	lastACK = seq;
	retv = sendFrame(sock, f, flags);
	if (retv < 0)
		printError();
	return retv;
}

int sendNACK(int sock, Frame *f, uint8_t seq, int flags) {
	ssize_t retv;
	mountNACK(f, seq);
	retv = sendFrame(sock, f, flags);
	if (retv < 0)
		printError();
	return retv;
}

int inLastNPackets(uint8_t seqn, uint8_t n) {
	uint8_t increment = 0x1F - frameseq;
	return (uint8_t) ((seqn + increment) % 32) >= (uint8_t) 0x1F - n; 
}

int inNextNPackets(uint8_t seqn, uint8_t n) {
	return ((seqn - frameseq) & 31) <= (n & 31); 
}

void mountFrameWithSeq(Frame *f, enum FrameType type, uint8_t dataSize, uint8_t *data, uint8_t seq) {
	f->type = type;
	f->sequence = seq;
	f->dataSize = dataSize;
	memcpy(f->data, data, dataSize);
	setFrameCRC(f);
}

int recvFrame(int sock, Frame *f, int flags) {
	static uint8_t windowidx = 0, windowmode = 0;
	Frame *_f;
	ssize_t bytesread;
	struct timeval start, limit;
	gettimeofday(&start, NULL);
	limit = timevalAdd(&start, &protocoltimeout);
	do {
		if ((bytesread = recv(sock, ethbuffer, ETH_FRAME_LEN, 0)) < 0) {
			if (errno != EAGAIN) // Filtra timeouts
				printError();
			continue;
		}
		if (bytesread < FRAME_MIN_TOTAL_SIZE || ((Frame *) ethbuffer)->startMarker != START_MARKER) {
			continue;
		}
		_f = (Frame *) ethbuffer;
		if (!checkCRC(_f)) {
			mountFrameWithSeq(f, NACK, sizeof(uint8_t), (uint8_t *) frameseqaddr, (frameseq+1)%32);
			sendFrame(sock, f, 0);
			// fprintf(stderr, "CRC %u failed, error detected.\n", _f->sequence);
			continue;
		}
		if (_f->type <= NACK) { // Se for ACK ou NACK
			// fprintf(stderr, "ACK/NACK %u inesperado.\n", _f->sequence);
			continue;
		}
		if (_f->sequence != frameseq) {
			continue;
		}
		frameseq++;
		TIMEOUT_RECOVER_METHOD();
#ifdef _LOG_FRAMES_
		printf("Frame %d recebido\n", _f->sequence);
#endif
		if (_f->type == FDESC) {
			windowmode = 1;
			sendACK(sock, f, _f->sequence, 0);
		}
		else if (windowmode) {
			if (_f->type != DATA) {
				sendACK(sock, f, _f->sequence, 0);
				windowmode = 0;
				windowidx = 0;
			}
			else {
				windowidx++;
				windowidx %= GO_BACK_WINDOW_SIZE;
				lastACK = _f->sequence;
				if (windowidx == 0) {
					sendACK(sock, f, _f->sequence, 0);
				}
			}
		}
		else {
			sendACK(sock, f, (frameseq - 1) % 32, 0);
		}
		// fprintf(stderr, "ACK sent %u.\n", _f->sequence);
		memcpy(f, _f, getFrameTotalSize(_f));
		return 1;
	} while (!verifyTimeout(limit));
	if (!(flags & RECV_NOTIMEOUT_INCREASE))
		increaseTimeout();
	f->type = TIMEOUT;

	return 0;
}

int recvACK(int sock, Frame *f, int flags) {
	Frame *_f;
	ssize_t bytesread;
	struct timeval start, limit;
	gettimeofday(&start, NULL);
	limit = timevalAdd(&start, &protocoltimeout);
	do {
		if ((bytesread = recv(sock, ethbuffer, ETH_FRAME_LEN, flags)) < 0) {
			if (errno != EAGAIN)
				printError();
			continue;
		}
		if (bytesread < FRAME_MIN_TOTAL_SIZE || ((Frame *) ethbuffer)->startMarker != START_MARKER) {
			continue;
		}
		_f = (Frame *) ethbuffer;
		if (!checkCRC(_f)) {
			continue;
		}
		if (_f->type > NACK) { // Se não for (N)ACK
			continue;
		}
#ifdef _LOG_FRAMES_
		printf("ACK %d recebido\n", _f->sequence);
#endif
		TIMEOUT_RECOVER_METHOD();
		memcpy(f, _f, getFrameTotalSize(_f));
		return 1;
	} while (!verifyTimeout(limit));
	increaseTimeout();
	f->type = TIMEOUT;

	return 0;
}

/*
 * Recebe ACK janela deslizante.
 */
int recvSWACK(int sock, Frame *f, int flags) {
	Frame *_f;
	ssize_t bytesread;
	struct timeval start, limit;
	gettimeofday(&start, NULL);
	limit = timevalAdd(&start, &protocoltimeout);
	do {
		if ((bytesread = recv(sock, ethbuffer, ETH_FRAME_LEN, flags)) < 0) {
			if (errno != EAGAIN) // Filtra timeouts
				printError();
			continue;
		}
		if (bytesread < FRAME_MIN_TOTAL_SIZE || ((Frame *) ethbuffer)->startMarker != START_MARKER) {
			continue;
		}
		_f = (Frame *) ethbuffer;

		if (!checkCRC(_f)) {
			// fprintf(stderr, "CRC %u failed, error detected.\n", _f->sequence);
			continue;
		}
		if  (_f->type > NACK) {
			continue;
		}
		if (!inNextNPackets(_f->sequence, GO_BACK_WINDOW_SIZE)) {
			// Se não estiver dentro da janela de resposta
#ifdef _LOG_FRAMES_
			printf("ACK recusado.\n");
#endif
			continue;
		}
		TIMEOUT_RECOVER_METHOD();
		memcpy(f, _f, getFrameTotalSize(_f));
		return 1;
	} while (!verifyTimeout(limit));
	increaseTimeout();
	f->type = TIMEOUT;

	return 0;
}

int sendFrameUntilACK(int sock, Frame *f, int flags) {
	Frame f1;
	Frame *frcvbuffer = &f1; // Ponteiro para buffer na stack
	initFrame(frcvbuffer);
	while (getTimeoutNumber() < 16) {
		sendFrame(sock, f, flags);	// Filename
		recvACK(sock, frcvbuffer, 0); // (N)ACK
		switch (frcvbuffer->type) {
		case ACK:
			uint8_t acknum = f->sequence;
			if (acknum == f->sequence)
				return 1;
#ifdef _LOG_FRAMES_
			else
				fprintf(stderr, "Expected ACK of %u, got ACK of %u.\n", f->sequence, acknum);
#endif
			break;
		case NACK:
			acknum = f->sequence;
#ifdef _LOG_FRAMES_
			fprintf(stderr, "Received NACK %u.\n", acknum);
#endif
			break;
		case TIMEOUT:
#ifdef _LOG_FRAMES_
			printf("Timeout.\n");
#endif
			break;
		default:
			fprintf(stderr, "Unexpected response for list command. Ignoring.\n");
			break;
		}
	}

	return 0;
}

int dataByteNeedsEscape(uint8_t b) {
	// Identificadores VLAN 0x8100 e 0x88A8
	// Byte de escape antes de 0x00, 0xA8 e ESCAPE_BYTE
	switch (b) {
	case 0x00:
	case 0xA8:
	case ESCAPE_BYTE:
		return 1;
	default:
		return 0;
	}
}

/// @brief Preenche um frame de dados @f com dados de @data.
/// @param f Ponteiro para um frame de dados, assumindo que o marcador de início e
/// tipo já estejam corretos.
/// @param data Ponteiro para dados.
/// @param dsize Tamanho total do buffer de dados.
/// @return Quantidade de bytes lidos de @data. Utilizado para o preenchimento
/// dos próximos frames.
size_t fillDataFrame(Frame *f, uint8_t *data, size_t dsize) {
	size_t bytesRead = (size_t) data;
	for (f->dataSize = 0; f->dataSize < FRAME_MAX_DATA && dsize > 0; dsize--, data++, f->dataSize++) {
		if (dataByteNeedsEscape(*data)) {
			if (f->dataSize > FRAME_MAX_DATA - 2) // Verifica se há espaço para dois bytes
				break; // Quebra o laço se não houver espaço no frame
			f->data[f->dataSize++] = ESCAPE_BYTE; // Adiciona byte de escape
		}
		f->data[f->dataSize] = *data; // Copia os dados para o frame
	}
	setFrameCRC(f);
	bytesRead = (size_t) data - bytesRead; // Data final - Data inicial - 1 (Valor de Data final não é lido ao fim do loop)
	return bytesRead;
}

/// @brief Lê os dados de um frame de dados @f e guarda os dados em @dst.
/// Remove bytes de escape. Espera-se que haja espaço suficiente para um
/// vetor de dados de um frame completamente cheio no vetor de destino.
/// Caso contrário ocorrerá um buffer overflow.
/// @param f Ponteiro para o frame de dados.
/// @param dst Ponteiro para o buffer de destino dos dados.
/// @param dstsize Tamanho total do buffer de destino.
/// @param dstused Total de bytes usados do buffer de destino.
/// @return Quantidade de bytes escritos no buffer de destino.
size_t readDataFrame(Frame *f, uint8_t *dst, size_t dstsize, size_t dstused) {
	size_t innitialdstused = dstused;
	for (int i = 0; i < f->dataSize && dstused < dstsize; i++, dstused++) {
		if (f->data[i] == ESCAPE_BYTE)
			i++;
		dst[dstused] = f->data[i];
	}
	return dstused - innitialdstused;
}

size_t fillDataFrameBuffer(FILE *fp, Frame *fbuffer, uint8_t bufsize, uint8_t *sentframe, uint8_t *data, size_t dsize, size_t *dused) {
	static size_t bytessent = 0;
	uint8_t floadidx;
	size_t bytesRead;
	uint8_t iterstart = 255;
	for (int i = 0; i < GO_BACK_WINDOW_SIZE; i++) {
		if (fbuffer[i].sequence == frameseq) {
			iterstart = i;
			break;
		}
	}
	// Só ocorre se houver um erro no programa
	if (iterstart == 255) {
		fprintf(stderr, "Error in assembly of sliding window. Aborting.\n");
		exit(1);
	}
	for (int i = 0; i < bufsize && dsize > 0; i++) {
		floadidx = (iterstart+i)%bufsize;
		if (!sentframe[floadidx]) {
			fprintf(stderr, "Frame %u not sent, need to send again.\n", fbuffer[floadidx].sequence);
			continue;
		}
		sentframe[floadidx] = 0; // Marca o frame como pronto para o envio
		bytesRead = fillDataFrame(fbuffer+floadidx, data + *dused, dsize);
		*dused += bytesRead;
		dsize -= bytesRead;
		bytessent += bytesRead;
		if (dsize == 0) {
			dsize = fread(data, 1, BUFSIZ, fp);
			*dused = 0;
		}
	}
	printf("%lu bytessent.\n", bytessent);
	return dsize;
}

uint8_t sendFrameWindow(int sock, Frame *fbuffer, uint8_t *sentframe) {
	size_t fsendidx;
	Frame resframe;
	Frame *response = &resframe;
	uint8_t gotValidResponse = 0;
	uint8_t acknum;
	uint8_t lastwindow = 0;
	uint8_t newseq;
	uint8_t iterstart = 255;
	int i = 0;
	for (i = 0; i < GO_BACK_WINDOW_SIZE; i++) {
		if (fbuffer[i].sequence == frameseq) {
			iterstart = i;
			break;
		}
	}
	// Só ocorre se houver um erro no programa
	if (iterstart == 255) {
		fprintf(stderr, "Error in assembly of sliding window.\n");
		exit(1);
	}
	
	while (!gotValidResponse && getTimeoutNumber() < 16) {
		// Envia a janela
		for (i = 0; i < GO_BACK_WINDOW_SIZE; i++) {
			fsendidx = (iterstart+i)%GO_BACK_WINDOW_SIZE;
			if (sentframe[fsendidx]) {
				lastwindow = 1;
				break;
			}
			sendFrame(sock, fbuffer+fsendidx, 0);
		}
		// Adiciona o frame de fim de transmissão na janela
		if (lastwindow) {
			mountFrameWithSeq(fbuffer+fsendidx, ENDTX, 0, NULL, (frameseq+i)%32);
			sendFrame(sock, fbuffer+fsendidx, 0);
		}

		// Recebe resposta ((N)ACK)
		recvSWACK(sock, response, 0);
		switch (response->type) {
		case ACK:
			acknum = response->sequence;
			if (!inNextNPackets(acknum, GO_BACK_WINDOW_SIZE)) {
				fprintf(stderr, "Unexpected ACK received: Expected ACK in [%u, %u], got %u.\n", frameseq, frameseq+GO_BACK_WINDOW_SIZE, acknum);
				break; // Switch
			}
			for (int i = 0; i < GO_BACK_WINDOW_SIZE; i++) {
				fsendidx = (iterstart+i)%GO_BACK_WINDOW_SIZE;
				if (!inNextNPackets(fbuffer[fsendidx].sequence, acknum - frameseq))
					break; // Loop
				newseq = fbuffer[fsendidx].sequence;
				newseq += GO_BACK_WINDOW_SIZE;
				newseq %= FRAME_SEQ_MAX;
				fbuffer[fsendidx].sequence = newseq;
				sentframe[fsendidx] = 1;
			}
			frameseq = acknum + 1;
			gotValidResponse = 1;
			break; // Switch
		case NACK:
			acknum = response->sequence;
			if (!inNextNPackets(acknum, GO_BACK_WINDOW_SIZE)) {
				fprintf(stderr, "Unexpected NACK received: Expected NACK in [%u, %u], got %u.\n", frameseq+1, frameseq+GO_BACK_WINDOW_SIZE, acknum);
				break; // Switch
			}
			for (int i = 0; i < GO_BACK_WINDOW_SIZE; i++) {
				fsendidx = (iterstart+i)%GO_BACK_WINDOW_SIZE;
				if (fbuffer[fsendidx].sequence == acknum)
					break; // Loop
				fbuffer[fsendidx].sequence += GO_BACK_WINDOW_SIZE;
				// sentframe[fsendidx] = 0; // (nao necessario porque o valor já é 0)
			}
			frameseq = acknum;
			gotValidResponse = 1;
			break; // Switch
		case TIMEOUT:
			break; // Switch
		default:
			fprintf(stderr, "Unexpected response packet type received in window transfer.\n");
			fprintf(stderr, "%s\n", frameTypeString(response->type));
			break; // Switch
		}
	}
	return lastwindow;
}

/*
 * Envia uma filestream do arquivo @fp pelo socket @sock utilizando o frame @fbuffer
 * para a janela deslizante.
 */
int sendDataStream(int sock, FILE *fp, Frame *fbuffer) {
	uint8_t *data = calloc(BUFSIZ, sizeof(uint8_t)), *sentframe = calloc(GO_BACK_WINDOW_SIZE, sizeof(uint8_t));
	uint8_t sentDataStream = 0; // Variável de controle para saber se todos os dados já foram enviados
	size_t dsize, dused = 0;
	if (!data || !sentframe) {
		free(data);
		free(sentframe);
		return 0;
	}
	memset(sentframe, 1, GO_BACK_WINDOW_SIZE);
	dsize = fread(data, 1, BUFSIZ, fp);
	for (int i = 0; i < GO_BACK_WINDOW_SIZE; i++) {
		fbuffer[(frameseq+i)%GO_BACK_WINDOW_SIZE].sequence = (frameseq+i)%32;
	}
	printf("Buffer loaded with %lu.\n", dsize);
	while (!sentDataStream) {
		dsize = fillDataFrameBuffer(fp, fbuffer, GO_BACK_WINDOW_SIZE, sentframe, data, dsize, &dused);
		sendFrameWindow(sock, fbuffer, sentframe);
		if (dsize == 0) { // Acabaram os dados do arquivo, resta somente dos frames carregados
			sentDataStream = 1;
			for (int i = 0; i < GO_BACK_WINDOW_SIZE; i++) { // Verifica se restam frames para enviar
				sentDataStream &= sentframe[i];
			}
		}
	}
	free(data);
	free(sentframe);
	printf("Finished File Stream.\n");
	
	return 1;
}

/*
 * Envia um arquivo de nome @fname para o socket @sock
 * Retorna 1 em sucesso e 0 em falha
 */
int sendFile(int sock, char *fname) {
	FILE *fp;
	struct stat filestat;
	Frame f, *fbuffer = newDataFrameBuffer(GO_BACK_WINDOW_SIZE);
	uint8_t errorcode = NOERROR;
	struct fdesc fdescriptor;
	int invalidResponse = 1;

	initFrame(&f);
	if ((fp = fopen(fname, "r")) == NULL || !fbuffer || stat(fname, &filestat) < 0) {
		printError();
		errorcode = getError();
		mountFrame(&f, ERROR, sizeof(uint8_t), &errorcode);
	}
	else {
		fdescriptor.size = filestat.st_size;
		fdescriptor.t.actime = filestat.st_atime;
		fdescriptor.t.modtime = filestat.st_mtime;
		mountFrame(&f, FDESC, sizeof(fdescriptor), (uint8_t *) &fdescriptor);
	}
	sendFrameUntilACK(sock, &f, 0);
	if (f.type == ERROR) {
		if (fp)
			fclose(fp);
		free(fbuffer);
		return 0;
	}
	while (invalidResponse && getTimeoutNumber() < 16) {
		recvFrame(sock, &f, 0);
		switch (f.type) {
		case ERROR:
			errorcode = (uint8_t) f.data[0];
			if (errorcode != NOERROR) {
				printErrorFrame(&f);
				fclose(fp);
				free(fbuffer);
				return 0;
			}
			invalidResponse = 0;
			break;
		case TIMEOUT:
			break;
		default:
			fprintf(stderr, "Unexpected packet received.\n");
			printFrame(&f);
			break;
		}
	}

	sendDataStream(sock, fp, fbuffer);
	free(fbuffer);
	fclose(fp);
	return getTimeoutNumber() >= 16;
}
