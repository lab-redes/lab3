#include <stdio.h>          // Librería estándar de entrada y salida
#include <stdlib.h>         // Librería general (malloc, exit, etc.)
#include <string.h>         // Manejo de cadenas (strlen, strcpy, strcmp, etc.)
#include <unistd.h>         // Funciones POSIX (close, read, write)
#include <arpa/inet.h>      // Librería para manejo de direcciones IP y funciones de red

#define PORT 5050
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr; // Estructura para almacenar la dirección del servidor
    char mensaje[BUFFER_SIZE], topic[50], header[60];

    // CREACIÓN DEL SOCKET DEL CLIENTE (PUBLISHER)
    // socket(): crea un endpoint de comunicación
    // AF_INET: familia de direcciones IPv4
    // SOCK_STREAM: indica conexión orientada a flujo (TCP)
    // 0: selecciona automáticamente el protocolo TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        return -1;
    }

    // CONFIGURACIÓN DE LA DIRECCIÓN DEL SERVIDOR
    // Se define la familia de direcciones y el puerto del broker
    serv_addr.sin_family = AF_INET;       // IPv4
    serv_addr.sin_port = htons(PORT);     // Conversión del número de puerto a formato de red

    // inet_pton(): convierte una dirección IP en formato texto ("127.0.0.1")
    // a formato binario y la almacena en serv_addr.sin_addr
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Dirección inválida");
        return -1;
    }

    // ESTABLECER CONEXIÓN TCP CON EL BROKER
    // connect(): inicia el proceso de conexión TCP (three-way handshake)
    // Si el servidor acepta la conexión, se establece un canal confiable y orientado a conexión
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error de conexión");
        return -1;
    }

    printf("Conectado al broker TCP en el puerto %d\n", PORT);

    // IDENTIFICAR EL TOPIC
    // El publicador se registra enviando un mensaje especial con el prefijo "PUB:"
    // que el broker usa para asociar este socket a un topic específico
    printf("Ingresa el topic al que publicarás (ej: futbol): ");
    fgets(topic, 50, stdin);
    topic[strcspn(topic, "\n")] = 0;              // Elimina salto de línea del final
    sprintf(header, "PUB:%s", topic);             // Construye el encabezado de registro
    send(sock, header, strlen(header), 0);        // Envía el encabezado al broker
    printf("Registrado como publisher del topic '%s'\n", topic);

    // ENVÍO DE MENSAJES AL BROKER
    // En este bucle, el publicador envía mensajes continuamente al broker
    // mediante send(), que escribe datos en el flujo TCP.
    while (1) {
        printf("> ");
        fgets(mensaje, BUFFER_SIZE, stdin);
        mensaje[strcspn(mensaje, "\n")] = 0; // Elimina salto de línea

        // Si el usuario escribe "exit", se rompe el bucle y se cierra la conexión
        if (strcmp(mensaje, "exit") == 0)
            break;

        // send(): envía los datos al servidor
        // TCP garantiza que los bytes lleguen completos y en el mismo orden
        send(sock, mensaje, strlen(mensaje), 0);
        printf("Mensaje enviado: %s\n", mensaje);
    }

    // CIERRE DE LA CONEXIÓN
    // close(): cierra el socket y notifica al broker que la conexión TCP ha terminado
    close(sock);
    printf("Conexión cerrada.\n");
    return 0;
}
