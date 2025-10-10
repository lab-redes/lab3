/* publisher_udp.c
 *
 * Publisher UDP minimal:
 * - Envia mensajes al broker (por defecto 127.0.0.1:5000).
 * - Uso:
 *     gcc publisher_udp.c -o publisher_udp
 *     ./publisher_udp <topic> [broker_ip] [broker_port]
 *
 *   Luego escribe líneas por stdin; cada línea será enviada como payload al broker.
 *
 * Formato de envío hacia broker:
 *   "PUB <topic> <payload>"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_BROKER_IP "127.0.0.1"
#define DEFAULT_BROKER_PORT 5000
#define BUF_SIZE 2048

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <topic> [broker_ip] [broker_port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* topic = argv[1];
    const char* broker_ip = (argc >= 3) ? argv[2] : DEFAULT_BROKER_IP;
    int broker_port = (argc >= 4) ? atoi(argv[3]) : DEFAULT_BROKER_PORT;

    int sockfd;
    struct sockaddr_in broker_addr;
    char line[BUF_SIZE];
    char out[BUF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[publisher] socket");
        exit(EXIT_FAILURE);
    }

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port);
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) != 1) {
        perror("[publisher] inet_pton broker_ip");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[publisher] Enviando a %s:%d en topic '%s'. Escribe mensajes y presiona Enter.\n",
        broker_ip, broker_port, topic);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        // quitar newline
        size_t L = strlen(line);
        if (L > 0 && line[L - 1] == '\n') line[L - 1] = '\0';
        // Construir mensaje
        snprintf(out, sizeof(out), "PUB %s %s", topic, line);
        ssize_t sent = sendto(sockfd, out, strlen(out), 0,
            (struct sockaddr*)&broker_addr, sizeof(broker_addr));
        if (sent < 0) {
            perror("[publisher] sendto");
        }
        else {
            printf("[publisher] Enviado (%zd bytes): %s\n", sent, line);
        }
    }

    close(sockfd);
    return 0;
}
