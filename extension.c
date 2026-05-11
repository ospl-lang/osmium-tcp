#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

/* SERVER STUFF */

int createServer(int port) {
    int sockfd;
    struct sockaddr_in address = {0};
    socklen_t address_size = sizeof(address);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(port);
    address.sin_family = AF_INET;

    if (bind(sockfd, (struct sockaddr*)&address, address_size) != 0) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, 100) != 0) {
        perror("listen");
        exit(1);
    }

    printf("listening on localhost:%i...\n", port);

    return sockfd;
}

int serverClose(int s) {
    close(s);
}

/* SERVER CLIENT STUFF */
typedef struct {
    int fd;

    socklen_t addr_len;
    struct sockaddr_in client_addr;

    unsigned char readbuf[BUFSIZ];
    unsigned char writbuf[BUFSIZ];
    unsigned long readcur;
    unsigned long writcur;
} ServerClient;

ServerClient* serverAccept(int s) {
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    ServerClient* c = malloc(sizeof(ServerClient));
    if (c == NULL) {
        perror("failed to allocate memory");
        exit(1);
    }

    int client_fd = accept(s,
        (struct sockaddr*)&client_addr,
        &len);

    if (client_fd == -1) {
        perror("failed to accept");
        free(c);
        exit(1);
    }

    c->fd = client_fd;
    c->addr_len = len;
    c->client_addr = client_addr;
    c->readcur = 0;
    c->writcur = 0;

    printf("accepted connection #%i\n", c->fd);

    return c;
}

unsigned char CRead(ServerClient* c) {
    int readed;

    if (c->readcur == 0 || c->readcur >= BUFSIZ) {
        c->readcur = 0;

        readed = recv(c->fd, c->readbuf, BUFSIZ, 0);
        if (readed <= 0) {
            perror("recv");
            return 0;
        }

        c->readbuf[readed] = 0;
    }

    unsigned char b = c->readbuf[c->readcur];
    c->readcur++;

    return b;
}

int CFlush(ServerClient *c) {
    size_t sent_total = 0;

    while (sent_total < c->writcur) {
        ssize_t sent = send(c->fd,
                            c->writbuf + sent_total,
                            c->writcur - sent_total,
                            0);

        if (sent <= 0) {
            return -1;
        }

        sent_total += sent;
    }

    c->writcur = 0;
    return 0;
}

int CWrite(ServerClient *c, const void *data, size_t len) {
    if (c->writcur + len > BUFSIZ) {
        if (CFlush(c) == -1) return -1;
    }

    memcpy(c->writbuf + c->writcur, data, len);
    c->writcur += len;

    return 0;
}

int CWrite1(ServerClient *c, unsigned char b) {
    c->writbuf[c->writcur++] = b;

    if (c->writcur == BUFSIZ) {
        return CFlush(c);
    }

    return 0;
}

void CClose(ServerClient* c) {
    shutdown(c->fd, SHUT_RDWR);
    close(c->fd);
    free(c);
}