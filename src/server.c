#include "server.h"

#define FRAMEBUFSIZE 8

int checkCLIErrors(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Argumentos inválidos.\nUso correto: ./server <iface_name>\n");
		return 1;
	}
	return 0;
}

int listVideos(int sock, Frame *fbuffer, Frame *frcvbuffer) {
	DIR *dfd = opendir("media");
	struct dirent *dent;
	size_t slen;
	printf("Request to list received.\n");
	if (!dfd) {
		fprintf(stderr, "Error at opening media directory. Server is badly configured\n");
		return 0;
	}
	while ((dent = readdir(dfd))) {
		if (strstr(dent->d_name, ".mp4") || strstr(dent->d_name, ".avi")) {
			slen = strlen(dent->d_name);
			if (slen < 63)
				slen++;
			mountFrame(fbuffer, SHOW, slen, (uint8_t *) dent->d_name);
			sendFrameUntilACK(sock, fbuffer, 0);
		}
	}
	mountFrame(fbuffer, ENDTX, 0, NULL);
	sendFrameUntilACK(sock, fbuffer, 0);
	
	closedir(dfd);
	
	return 1;
}

int downloadVideo(int sock, Frame *f) {
	char fname[FRAME_MAX_DATA+1+sizeof("media/")] = "media/";
	strncat(fname, (char *) f->data, FRAME_MAX_DATA);
	fname[FRAME_MAX_DATA+sizeof("media/")] = '\0'; // Garante que a string tenha um fim.
	printf("Request to download file: %s\n", fname);
	return sendFile(sock, fname);
}

void runServer(int sock) {
	// Bloco único alocado para 3 variáveis
	Frame *frcv = newFrames(2); // Ponteiro para um frame (utilizado para receber frames)
	Frame *fbuffer = frcv+1; // Ponteiro para um frame (utilizado para enviar frames)
	srand(0); // Utilizado para gerar delays aleatorios caso seja desejado
	if (!frcv) {
		fprintf(stderr, "Memory error.\n");
		free(frcv);
		return;
	}
	printf("Server is running\n");
	while (1) {
		recvFrame(sock, frcv, RECV_NOTIMEOUT_INCREASE);
		switch (frcv->type) {
		case LIST:
			listVideos(sock, fbuffer, frcv);
			break;
		case DOWNLOAD:
			downloadVideo(sock, frcv);
			break;
		case TIMEOUT:
			break;
		default:
			fprintf(stderr, "Unexpected packet type received.\n");
			break;
		}
		resetTimeout(); // Impede o aumento do timeout durante o modo de espera
	}

	free(frcv);
}

int main(int argc, char **argv) {
	int sock;
	if (checkCLIErrors(argc, argv))
		return 1;
	sock = newSocket(argv[1]);
	runServer(sock);

	close(sock);
	return 0;
}
