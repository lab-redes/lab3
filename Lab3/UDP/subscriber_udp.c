//   gcc subscriber_udp.c -o subscriber_udp
//   ./subscriber_udp <topic> [broker_ip] [broker_port]


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_BROKER_IP "127.0.0.1"   // ip por defecto del broker (localhost)
#define DEFAULT_BROKER_PORT 5000        // puerto por defecto del broker
#define BUF_SIZE 2048                   // tamaño del buffer

int main(int argc, char *argv[]) {
    if (argc < 2) {
        // si no se pasa el topic, muestra como usar el programa
        fprintf(stderr, "Uso: %s <topic> [broker_ip] [broker_port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *topic = argv[1];   // el primer argumento es el topic
    const char *broker_ip = (argc >= 3) ? argv[2] : DEFAULT_BROKER_IP; // ip del broker (si no se pasa usa la por defecto)
    int broker_port = (argc >= 4) ? atoi(argv[3]) : DEFAULT_BROKER_PORT; // puerto del broker

    int sockfd;                      // descriptor del socket
    struct sockaddr_in local_addr, broker_addr;  // direcciones local y del broker
    char buf[BUF_SIZE];              // buffer pra los mensajes que se reciban

    // crear socket udp (SOCK_DGRAM). si da error se sale
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[subscriber] socket");
        exit(EXIT_FAILURE);
    }

    // bind a puerto 0 (significa que el sistema elige el puerto disponible)
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;                 // ipv4
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // esccha en todas las interfaces (0.0.0.0)
    local_addr.sin_port = htons(0);                  // el SO escoge el puerto

    // asocia el socket con la dirección local
    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("[subscriber] bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // obtiene el puerto real que le asignó el sistema (por que pusimos 0 antes)
    socklen_t len = sizeof(local_addr);
    if (getsockname(sockfd, (struct sockaddr*)&local_addr, &len) == -1) {
        perror("[subscriber] getsockname");
    }

    // preparar la dirección del broker
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port);
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) != 1) {
        // inet_pton convirte la ip de texto a binario (para sockaddr_in)
        perror("[subscriber] inet_pton broker_ip");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // enviar el mensaje SUB al broker para suscribirse al topic
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "SUB %s", topic);
    ssize_t sent = sendto(sockfd, msg, strlen(msg), 0,
                          (struct sockaddr*)&broker_addr, sizeof(broker_addr));
    if (sent < 0) {
        perror("[subscriber] sendto SUB");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // mostrar la info local (ip y puerto que se usa)
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ipstr, sizeof(ipstr));
    printf("[subscriber] Suscrito a '%s'. Escuchando en %s:%d\n",
           topic, ipstr, ntohs(local_addr.sin_port));

    // bucle infinto donde se reciben los mensajes del broker
    while (1) {
        struct sockaddr_in src;       // dirección del que envía el mensaje
        socklen_t addrlen = sizeof(src);
        ssize_t r = recvfrom(sockfd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr*)&src, &addrlen);
        if (r < 0) {
            // si hay error en la recepción se muestra y se sigue
            perror("[subscriber] recvfrom");
            continue;
        }

        buf[r] = '\0';  // se añade el fin de cadena al mensaje recibido

        // convierte la ip dl remitente a texto
        char srcip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, srcip, sizeof(srcip));

        // muestra el mensaje recibido por consola
        printf("[subscriber] Mensaje desde %s:%d -> %s\n", srcip, ntohs(src.sin_port), buf);
    }

    // aunque este punto nunca se alcanza, por buena practica se cierra el socket
    close(sockfd);
    return 0;
}
