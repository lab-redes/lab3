
//     gcc publisher_udp.c -o publisher_udp
//     ./publisher_udp <topic> [broker_ip] [broker_port]


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_BROKER_IP "127.0.0.1"   // ip del broker por defecto (localhost)
#define DEFAULT_BROKER_PORT 5000        // puerto del broker pr defecto
#define BUF_SIZE 2048                   // tamaño max del buffer

int main(int argc, char* argv[]) {
    if (argc < 2) {
        // si no se pone al menos el topic, muestra como se usa y sale
        fprintf(stderr, "Uso: %s <topic> [broker_ip] [broker_port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // el primer argumento es el tema al que se publicará
    const char* topic = argv[1];
    // si el usuario pone una ip la toma, sino usa la de defecto
    const char* broker_ip = (argc >= 3) ? argv[2] : DEFAULT_BROKER_IP;
    // si el usurio pone puerto lo usa, sino 5000
    int broker_port = (argc >= 4) ? atoi(argv[3]) : DEFAULT_BROKER_PORT;

    int sockfd;                         // descriptor del socket (como un id)
    struct sockaddr_in broker_addr;     // estrctura con la info del broker
    char line[BUF_SIZE];                // buffer donde se guarda lo que se escribe
    char out[BUF_SIZE];                 // buffer donde se arma el mensaje final

    // crear el socket udp, si da error muestra y termina
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[publisher] socket");
        exit(EXIT_FAILURE);
    }

    // poner en cero la estructura del broker (limpia memoria)
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;                // ipv4
    broker_addr.sin_port = htons(broker_port);       // convierte el puerto al formato de red
    // convertir la ip de texto a bnario
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) != 1) {
        perror("[publisher] inet_pton broker_ip");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[publisher] Enviando a %s:%d en topic '%s'. Escribe mensajes y presiona Enter.\n",
        broker_ip, broker_port, topic);

    // ciclo pricipal, lee lo que el usuario escribe por consola
    while (fgets(line, sizeof(line), stdin) != NULL) {
        // quitar el salto de línea (\n) al final del texto
        size_t L = strlen(line);
        if (L > 0 && line[L - 1] == '\n') line[L - 1] = '\0';

        // arma el mensaje completo que se mandaar al broker
        // ej: "PUB partido1 gol del equipo A"
        snprintf(out, sizeof(out), "PUB %s %s", topic, line);

        // envia el mensaje al broker usando UDP
        ssize_t sent = sendto(sockfd, out, strlen(out), 0,
            (struct sockaddr*)&broker_addr, sizeof(broker_addr));

        if (sent < 0) {
            // si sendto devuelve error lo musetra
            perror("[publisher] sendto");
        }
        else {
            // muestra que el mensaje se envió y cuantos bytes fueron
            printf("[publisher] Enviado (%zd bytes): %s\n", sent, line);
        }
    }

    // cuando se termina de escribir (ctrl+d) se cierra el socket
    close(sockfd);
    return 0;
}
