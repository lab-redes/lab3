#include <stdio.h>          // Librería estándar de entrada y salida
#include <stdlib.h>         // Librería general (malloc, exit, etc.)
#include <string.h>         // Manejo de cadenas (strlen, strcpy, strcmp, etc.)
#include <unistd.h>         // Funciones POSIX (close, read, write)
#include <arpa/inet.h>      // Librería para manejo de direcciones IP y funciones de red

#define PORT 5050
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr; // Estructura para almacenar la dirección del servidor (broker)
    char buffer[BUFFER_SIZE], topic[50], header[60];

    // CREACIÓN DEL SOCKET DEL CLIENTE (SUBSCRIBER)
    // socket(): crea un endpoint de comunicación
    // AF_INET: familia de direcciones IPv4
    // SOCK_STREAM: indica que es un socket orientado a conexión (TCP)
    // 0: selecciona automáticamente el protocolo TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        return -1;
    }

    // CONFIGURACIÓN DE LA DIRECCIÓN DEL SERVIDOR (BROKER)
    serv_addr.sin_family = AF_INET;       // IPv4
    serv_addr.sin_port = htons(PORT);     // Conversión del número de puerto a formato de red (big-endian)

    // inet_pton(): convierte la dirección IP en formato texto ("127.0.0.1")
    // a formato binario y la guarda en serv_addr.sin_addr
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Dirección inválida");
        return -1;
    }

    // ESTABLECER CONEXIÓN TCP CON EL BROKER
    // connect(): inicia el proceso de conexión (three-way handshake)
    // Si el servidor acepta, se establece un canal confiable y ordenado.
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error de conexión");
        return -1;
    }

    printf("Suscriptor conectado al broker TCP en el puerto %d\n", PORT);
    printf("Ingresa el topic al que deseas suscribirte (ej: futbol): ");
    fgets(topic, 50, stdin);
    topic[strcspn(topic, "\n")] = 0; // Elimina salto de línea

    // IDENTIFICACIÓN DEL SUBSCRIPTOR
    // Se construye un encabezado "SUB:<topic>" que el broker interpreta para registrar la suscripción
    sprintf(header, "SUB:%s", topic);
    send(sock, header, strlen(header), 0); // Envía el mensaje de suscripción al broker
    printf("Suscrito al topic '%s'\n", topic);
    printf("Esperando mensajes del broker...\n\n");

    // RECEPCIÓN DE MENSAJES DESDE EL BROKER
    // En este bucle, el suscriptor espera mensajes que el broker le reenvía
    // provenientes del publicador del mismo topic.
    while (1) {
        // read(): lee los datos del socket TCP
        // TCP garantiza que los datos lleguen completos y en el orden correcto
        int valread = read(sock, buffer, BUFFER_SIZE - 1);

        if (valread > 0) {
            buffer[valread] = '\0'; // Agrega terminador de cadena
            printf("[%s] %s\n", topic, buffer);
        } else if (valread == 0) {
            // Si el broker cierra la conexión, el valor devuelto es 0
            printf("Conexión cerrada por el broker.\n");
            break;
        } else {
            // Si ocurre un error en la lectura
            perror("Error al leer del socket");
            break;
        }
    }

    // CIERRE DE LA CONEXIÓN
    // close(): finaliza la conexión TCP con el broker
    close(sock);
    return 0;
}
