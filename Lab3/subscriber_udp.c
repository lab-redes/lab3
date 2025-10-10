/* subscriber_udp.c
 *
 * Subscriber UDP minimal:
 * - Se suscribe a un topic enviando "SUB <topic>" al broker (127.0.0.1:5000 por defecto).
 * - Luego escucha en el mismo socket para recibir mensajes reenviados por el broker.
 *
 * Uso:
 *   gcc subscriber_udp.c -o subscriber_udp
 *   ./subscriber_udp <topic> [broker_ip] [broker_port]
 *
 * Ejemplo:
 *   ./subscriber_udp partido1
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <topic> [broker_ip] [broker_port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *topic = argv[1];
    const char *broker_ip = (argc >= 3) ? argv[2] : DEFAULT_BROKER_IP;
    int broker_port = (argc >= 4) ? atoi(argv[3]) : DEFAULT_BROKER_PORT;

    int sockfd;
    struct sockaddr_in local_addr, broker_addr;
    char buf[BUF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[subscriber] socket");
        exit(EXIT_FAILURE);
    }

    // bind a puerto 0 (ephemeral) para recibir mensajes; esto permite que el broker mande a este puerto
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(0); // puerto elegido por el SO

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("[subscriber] bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // obtener el puerto asignado para mostrarlo
    socklen_t len = sizeof(local_addr);
    if (getsockname(sockfd, (struct sockaddr*)&local_addr, &len) == -1) {
        perror("[subscriber] getsockname");
    }

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port);
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) != 1) {
        perror("[subscriber] inet_pton broker_ip");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Enviar mensaje de suscripción desde este socket (asi el broker conoce la addr:port)
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "SUB %s", topic);
    ssize_t sent = sendto(sockfd, msg, strlen(msg), 0,
                          (struct sockaddr*)&broker_addr, sizeof(broker_addr));
    if (sent < 0) {
        perror("[subscriber] sendto SUB");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ipstr, sizeof(ipstr));
    printf("[subscriber] Suscrito a '%s'. Escuchando en %s:%d\n",
           topic, ipstr, ntohs(local_addr.sin_port));

    // Escuchar mensajes reenviados por el broker
    while (1) {
        struct sockaddr_in src;
        socklen_t addrlen = sizeof(src);
        ssize_t r = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr*)&src, &addrlen);
        if (r < 0) {
            perror("[subscriber] recvfrom");
            continue;
        }
        buf[r] = '\0';
        // Mostrar mensaje recibido
        char srcip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, srcip, sizeof(srcip));
        printf("[subscriber] Mensaje desde %s:%d -> %s\n", srcip, ntohs(src.sin_port), buf);
    }

    close(sockfd);
    return 0;
}
