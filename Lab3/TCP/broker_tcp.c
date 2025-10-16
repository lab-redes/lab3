#include <stdio.h>          // Librería estándar de entrada y salida
#include <stdlib.h>         // Librería para funciones generales (malloc, exit, etc.)
#include <string.h>         // Librería para manejo de cadenas (strcmp, strcpy, etc.)
#include <unistd.h>         // Para funciones POSIX como close(), read(), write()
#include <arpa/inet.h>      // Define estructuras y funciones para manejo de direcciones IP (inet_ntoa, htons, etc.)
#include <sys/select.h>     // Permite el uso de select(), que monitorea múltiples sockets a la vez
#include <errno.h>          // Permite manejar errores del sistema mediante la variable global errno

#define PORT 5050
#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define MAX_TOPICS 10

// Estructura para manejar suscriptores asociados a un "topic" (tema)
struct Topic {
    char name[50];
    int subscribers[MAX_CLIENTS]; // Guarda los descriptores de socket de cada suscriptor
};

// Estructura para los publishers
struct Publisher {
    int socket;      // Descriptor de socket del publicador
    char topic[50];  // Nombre del tema que publica
};

int main() {
    int server_fd, new_socket, client_sockets[MAX_CLIENTS];
    struct sockaddr_in address; // Estructura que almacena la dirección del servidor
    int max_sd, activity, valread;
    fd_set readfds;             // Conjunto de descriptores de archivo monitoreados por select()
    char buffer[BUFFER_SIZE];
    int addrlen = sizeof(address);

    struct Topic topics[MAX_TOPICS] = {0};
    struct Publisher publishers[MAX_CLIENTS] = {0};

    // Inicializa la lista de clientes
    for (int i = 0; i < MAX_CLIENTS; i++)
        client_sockets[i] = 0;

    // CREACIÓN DEL SOCKET DEL SERVIDOR
    // socket(): crea un endpoint de comunicación
    // AF_INET: familia de direcciones IPv4
    // SOCK_STREAM: tipo de socket orientado a conexión (TCP)
    // 0: selecciona el protocolo TCP automáticamente
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // setsockopt(): permite reutilizar la dirección si el socket se cierra
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configura la estructura de dirección del servidor
    address.sin_family = AF_INET;         // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Escucha en cualquier interfaz disponible
    address.sin_port = htons(PORT);       // Convierte el número de puerto a formato de red

    // ENLAZAR EL SOCKET A LA DIRECCIÓN LOCAL
    // bind(): asigna la dirección IP y puerto al socket del servidor
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    // ESCUCHAR CONEXIONES ENTRANTES
    // listen(): pone el socket en modo pasivo, esperando conexiones entrantes
    // MAX_CLIENTS: número máximo de conexiones en cola
    listen(server_fd, MAX_CLIENTS);

    printf("Broker TCP en ejecución. Escuchando en el puerto %d...\n", PORT);

    // Bucle principal del servidor
    while (1) {
        // Limpia y configura el conjunto de descriptores
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);  // Agrega el socket del servidor al conjunto
        max_sd = server_fd;

        // Agrega los sockets activos de clientes al conjunto
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // SELECT: MONITOREA LOS SOCKETS
        // select(): bloquea hasta que uno o más sockets estén listos para lectura
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR))
            perror("Error en select()");

        // NUEVA CONEXIÓN ENTRANTE
        if (FD_ISSET(server_fd, &readfds)) {
            // accept(): acepta una nueva conexión TCP entrante
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            printf("Nueva conexión desde %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // Guarda el nuevo socket en el arreglo de clientes
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        // PROCESAR DATOS EN SOCKETS EXISTENTES
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                // read(): lee datos del socket TCP
                valread = read(sd, buffer, BUFFER_SIZE - 1);
                if (valread <= 0) {
                    // Si la conexión se cerró o hubo error
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0'; // Añade terminador de cadena

                    // REGISTRO DE UN PUBLISHER
                    if (strncmp(buffer, "PUB:", 4) == 0) {
                        char topic[50];
                        sscanf(buffer, "PUB:%s", topic);
                        strcpy(publishers[i].topic, topic);
                        publishers[i].socket = sd;
                        printf("Publisher registrado en topic: %s\n", topic);
                    }

                    // REGISTRO DE UN SUBSCRIBER
                    else if (strncmp(buffer, "SUB:", 4) == 0) {
                        char topic[50];
                        sscanf(buffer, "SUB:%s", topic);
                        for (int t = 0; t < MAX_TOPICS; t++) {
                            // Si el topic existe o está vacío, se asigna
                            if (strcmp(topics[t].name, topic) == 0 || topics[t].name[0] == '\0') {
                                strcpy(topics[t].name, topic);
                                topics[t].subscribers[i] = sd;
                                break;
                            }
                        }
                        printf("Subscriber suscrito a topic: %s\n", topic);
                    }

                    // MENSAJE DE UN PUBLISHER
                    else {
                        char topic_pub[50] = "";
                        for (int p = 0; p < MAX_CLIENTS; p++) {
                            if (publishers[p].socket == sd) {
                                strcpy(topic_pub, publishers[p].topic);
                                break;
                            }
                        }

                        // Si no hay topic asociado, no se reenvía
                        if (strlen(topic_pub) == 0)
                            continue;

                        // Reenvía el mensaje a todos los suscriptores del mismo topic
                        for (int t = 0; t < MAX_TOPICS; t++) {
                            if (strcmp(topics[t].name, topic_pub) == 0) {
                                for (int s = 0; s < MAX_CLIENTS; s++) {
                                    int dest = topics[t].subscribers[s];
                                    if (dest != 0 && dest != sd) {
                                        // send(): envía los datos al socket destino
                                        send(dest, buffer, strlen(buffer), 0);
                                    }
                                }
                                break;
                            }
                        }

                        printf("[%s] %s\n", topic_pub, buffer);
                    }
                }
            }
        }
    }
}
