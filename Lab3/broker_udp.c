/* broker_udp.c
 *
 * Broker UDP simple para un sistema publish-subscribe por topics.
 * - Escucha en UDP port 5000 (por defecto).
 * - Recibe mensajes de tipo:
 *     SUB <topic>            -> registra al subscriber (por su addr)
 *     PUB <topic> <payload>  -> reenvía <payload> a todos los subscribers del <topic>
 *
 * Uso:
 *   gcc broker_udp.c -o broker_udp
 *   ./broker_udp            # escucha en 0.0.0.0:5000
 *
 * Nota: no usa librerías externas, solo API POSIX sockets y select().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BROKER_PORT 5000
#define BUF_SIZE 2048
#define MAX_SUBSCRIBERS 256
#define MAX_TOPIC_LEN 128

typedef struct {
    struct sockaddr_in addr;   // dirección del subscriber
    char topic[MAX_TOPIC_LEN]; // topic al que está suscrito
    int used;
} subscriber_t;

subscriber_t subscribers[MAX_SUBSCRIBERS];

/* Compara dos sockaddr_in (ip y puerto) */
int same_addr(const struct sockaddr_in* a, const struct sockaddr_in* b) {
    return (a->sin_family == b->sin_family) &&
        (a->sin_addr.s_addr == b->sin_addr.s_addr) &&
        (a->sin_port == b->sin_port);
}

/* Añade un subscriber (si no existe ya) */
void add_subscriber(const struct sockaddr_in* addr, const char* topic) {
    for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (subscribers[i].used) {
            if (strcmp(subscribers[i].topic, topic) == 0 &&
                same_addr(&subscribers[i].addr, addr)) {
                // ya registrado
                return;
            }
        }
        else {
            // usar este slot libre
            subscribers[i].used = 1;
            subscribers[i].addr = *addr;
            strncpy(subscribers[i].topic, topic, MAX_TOPIC_LEN - 1);
            subscribers[i].topic[MAX_TOPIC_LEN - 1] = '\0';
            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, ipstr, sizeof(ipstr));
            printf("[broker] Nuevo subscriber %s:%d para topic '%s'\n",
                ipstr, ntohs(addr->sin_port), subscribers[i].topic);
            return;
        }
    }
    fprintf(stderr, "[broker] Advertencia: lista de subscribers llena, no se puede agregar más.\n");
}

/* Envía payload a todos los subscribers del topic */
void forward_to_topic(int sockfd, const char* topic, const char* payload) {
    for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (!subscribers[i].used) continue;
        if (strcmp(subscribers[i].topic, topic) == 0) {
            ssize_t sent = sendto(sockfd, payload, strlen(payload), 0,
                (struct sockaddr*)&subscribers[i].addr,
                sizeof(subscribers[i].addr));
            if (sent < 0) {
                perror("[broker] sendto");
            }
            else {
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &subscribers[i].addr.sin_addr, ipstr, sizeof(ipstr));
                printf("[broker] Reenviado a %s:%d topic='%s' (%zd bytes)\n",
                    ipstr, ntohs(subscribers[i].addr.sin_port), topic, sent);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int sockfd;
    struct sockaddr_in broker_addr;
    char buf[BUF_SIZE];
    fd_set readfds;
    int maxfd;
    struct timeval tv;

    // inicializar lista de subscribers
    memset(subscribers, 0, sizeof(subscribers));

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[broker] socket");
        exit(EXIT_FAILURE);
    }

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    broker_addr.sin_port = htons(BROKER_PORT);

    if (bind(sockfd, (struct sockaddr*)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("[broker] bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[broker] Escuchando UDP en 0.0.0.0:%d\n", BROKER_PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        maxfd = sockfd;

        // opcionalmente podemos usar timeout para tareas periódicas; aquí bloqueamos hasta evento
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int rv = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (rv < 0) {
            perror("[broker] select");
            break;
        }
        else if (rv == 0) {
            // timeout: no hay datos; continue (podríamos hacer limpieza periódica si fuera necesario)
            continue;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);
            ssize_t len = recvfrom(sockfd, buf, BUF_SIZE - 1, 0,
                (struct sockaddr*)&src_addr, &addrlen);
            if (len < 0) {
                perror("[broker] recvfrom");
                continue;
            }
            buf[len] = '\0';

            // Decodificar mensaje: esperamos inicio con "SUB " o "PUB "
            if (len >= 4 && strncmp(buf, "SUB ", 4) == 0) {
                // SUB <topic>
                char topic[MAX_TOPIC_LEN];
                if (sscanf(buf + 4, "%127s", topic) == 1) {
                    add_subscriber(&src_addr, topic);
                }
                else {
                    fprintf(stderr, "[broker] SUB inválido: '%s'\n", buf);
                }
            }
            else if (len >= 4 && strncmp(buf, "PUB ", 4) == 0) {
                // PUB <topic> <payload...>
                char topic[MAX_TOPIC_LEN];
                // buscamos primer espacio después del topic
                char* p = buf + 4;
                if (sscanf(p, "%127s", topic) >= 1) {
                    // Avanzamos p hasta después del topic
                    p += strlen(topic);
                    while (*p == ' ') p++;
                    const char* payload = p;
                    if (*payload == '\0') {
                        fprintf(stderr, "[broker] PUB sin payload\n");
                    }
                    else {
                        // reenviar payload tal cual a los suscriptores del topic
                        forward_to_topic(sockfd, topic, payload);
                    }
                }
                else {
                    fprintf(stderr, "[broker] PUB inválido: '%s'\n", buf);
                }
            }
            else {
                // Mensaje desconocido: ignorar o logear
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &src_addr.sin_addr, ipstr, sizeof(ipstr));
                printf("[broker] Mensaje desconocido desde %s:%d --> %s\n",
                    ipstr, ntohs(src_addr.sin_port), buf);
            }
        }
    }

    close(sockfd);
    return 0;
}
