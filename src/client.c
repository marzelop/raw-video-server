#include "client.h"

#define FRAMEBUFSIZE 8
#define MAX_FILENAME_LEN 63

int checkCLIErrors(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Argumentos inválidos.\nUso correto: ./client <iface_name>\n");
		return 1;
	}
	return 0;
}

int listMedia(int sock, Frame *fbuffer) {
	uint8_t endTransmition = 0;
	printf("Listing:\n");
	mountFrame(fbuffer, LIST, 0, NULL);
	if (!sendFrameUntilACK(sock, fbuffer, 0)) {
		printf("Error at listing available media.\n");
		return 1;
	}
	while (!endTransmition && getTimeoutNumber() < 16) {
		recvFrame(sock, fbuffer, 0);
		switch (fbuffer->type) {
		case SHOW:
			printf("%.*s\n", MAX_FILENAME_LEN, fbuffer->data);
			break;
		case ENDTX:
			endTransmition = 1;
			break;
		case TIMEOUT:
			break;
		default:
			fprintf(stderr, "Unexpected message received at listing.\n");
			printFrame(fbuffer);
			printf("\n");
			break;
		}
	}
	printf("\n");
	return 0;
}

int receiveDataStream(int sock, FILE *fp, Frame *fbuffer) {
	static uint8_t data[BUFSIZ];
	size_t dsizeused = 0;
	uint8_t transmission = 1;
	while (transmission && getTimeoutNumber() < 16) {
		recvFrame(sock, fbuffer, 0);
		switch (fbuffer->type) {
		case DATA:
			dsizeused += readDataFrame(fbuffer, data, BUFSIZ, dsizeused);
			if (dsizeused > BUFSIZ - FRAME_MAX_DATA) {
				fwrite(data, 1, dsizeused, fp);
				dsizeused = 0;
			}
			break;
		case ENDTX:
			transmission = 0;
			fwrite(data, 1, dsizeused, fp);
			break;
		case TIMEOUT:
			repeatLastACK(sock, 0);
			printf("Timeout servidor burro!\n");
			break;
		
		default:
			break;
		}
	}
	fclose(fp);
	return 1;
}

// Arquivo é salvo como sendo do usuário root porque tem que rodar com sudo
int downloadVideo(int sock, Frame *fbuffer, int play) {
	FILE *fp;
	size_t slen;
	uint8_t invalidPacket = 1, errorcode;
	struct fdesc filedesc;
	static char commandbuffer[MAX_FILENAME_LEN+1+sizeof("celluloid  2> /dev/null &")];
	char buf[MAX_FILENAME_LEN+1];
	printf("Type the filename of the video you want to download.\n");
	if (scanf("\n")) { 
		// Remove espaço antes do nome digitado caso 
		// digite a opção e o nome do arquivo direto 
		// fflush(stdin) nao funciona
		// If para evitar warning
	}
	while (fgets(buf, sizeof(buf), stdin) == NULL) {
		printf("Failed reading filename.\n");
	}
	slen = strlen(buf);
	slen--;
	buf[slen] = '\0';
	if (slen < 63)
		slen++;
	mountFrame(fbuffer, DOWNLOAD, slen, (uint8_t *) buf);
	sendFrameUntilACK(sock, fbuffer, 0);

	while (invalidPacket && getTimeoutNumber() < 16) {
		recvFrame(sock, fbuffer, 0);
		switch (fbuffer->type) {
		case FDESC:
			invalidPacket = 0;
			memcpy(&filedesc, fbuffer->data, sizeof(filedesc));
			printf("File size: %lu\n", filedesc.size);
			errorcode = NOERROR;
			break;
		case ERROR:
			invalidPacket = 0;
			printErrorFrame(fbuffer);
			return 0;
			break;
		case TIMEOUT:
			break;
		default:
			fprintf(stderr, "Received unexpected packet. Aborting.\n");
			return 0;
			break;
		}
	}
	fp = fopen(buf, "w");
	if (!fp) {
		printError();
		errorcode = getError();
	}
	mountFrame(fbuffer, ERROR, sizeof(uint8_t), &errorcode);
	sendFrameUntilACK(sock, fbuffer, 0);
	if (errorcode != NOERROR)
		return 0;
	if (receiveDataStream(sock, fp, fbuffer)) {
		utime(buf, &filedesc.t);
		if (play) {
			printf("Playing %s.\n", buf);
			sprintf(commandbuffer, "celluloid %.63s 2> /dev/null", buf);
			if  (system(commandbuffer) != 0)
				printf("Failed to play video.\n");
			// Por algum motivo o programa para após fechar o player
		}
		return 1;
	}
	return 0;
}

void runClient(int sock) {
	Frame *frames = newFrames(FRAMEBUFSIZE);
	Frame *fbuffer = newFrames(1);
	uint8_t *buffer = calloc(ETH_FRAME_LEN, sizeof(uint8_t));
	int opt;
	srand(0); // Utilizado para gerar delays aleatorios caso seja desejado
	if (!frames || !buffer || !fbuffer) {
		fprintf(stderr, "Erro ao alocar memória.\n");
		free(frames);
		free(buffer);
		free(fbuffer);
		return;
	}
	while (1) {
		printf("Choose one:\n1 - List available media\n2 - Download video\n3 - Download and play video\n4 - Exit\n");
		while (scanf("\n%d", &opt) == 0)
			printf("Failed reading desired option.\n");
		printf("\n");
		switch (opt) {
		case 1:
			listMedia(sock, fbuffer);
			break;
		case 2:
			downloadVideo(sock, fbuffer, 0);
			break;
		case 3:
			downloadVideo(sock, fbuffer, 1);
		case 4:
			goto break_client_loop;
		default:
			printf("Unexpected option. %d\n", opt);
			break;
		}
		resetTimeout();
	}
	break_client_loop:
	free(frames);
	free(buffer);
	free(fbuffer);
}

int main(int argc, char **argv) {
	int sock;
	if (checkCLIErrors(argc, argv))
		return 1;
	sock = newSocket(argv[1]);
	runClient(sock);

	close(sock);
	return 0;
}
