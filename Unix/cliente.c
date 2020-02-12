/**
 * @file cliente.c
 * @author Ezequiel Zimmel (ezequielzimmel@gmail.com)
 * @brief Implemetacion de socket UNIX. El cliente funciona como satelite respondiendo 
 *        a las peticiones que realiza el servidor.
 * 	      Comienza creando un socket Unix orientado a la conexión. El usuario debe 
 *        ingresar como argumento el nombre del socket a utilizar (creado primero por el 
 *        servidor). ./<ejecutable> <socket>, ejemplo ./cliente server 
 *        Una vez conectado al servidor queda a la espera de comandos de operacion.
 * 
 * @version 0.1
 * @date 2020-01-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#define HORA 3600
#define MIN 60
#define SIZE 4096
#define TAM 80
#define TAM2 150
#define BUFSIZE 1024
#define BUFF_SIZE 1024
#define BYTES_STREAM 1500
#define FILE_BUFFER_SIZE 1500
#define DIRECTORIO_IMAGEN "/"
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

/* Librerias usados por los distintos codigos fuente */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/unistd.h>
#include <linux/kernel.h>

/* Funciones definidas */
int conectar(char *);
void sesionActiva(int, char *, char *);
void update_Firmware(int, char *, char *);
int start_Scanning(int);
int obtener_Telemetria(int, char *);
void getfirmware_version(char *);
void memoria(char *);
void bootTime(char *);
void uptime(char *);
void CPU(char *);
void hostname(char *);
void getValue(char *, char *, char *);

/**
 * @brief Llama a la funcion conectar. Si la conexión es posible
 *        activa la sesión con el servidor mediante el socket devuelto
 *        y se mantiene hasta que el servidor finaliza la sesion.
 * 
 * @param argc 
 * @param argv argv[0] nombre del ejecutable. Empleado en la funcion update_firmware.
 *             argv[1] nombre del socket UNIX. Medio de comunicación de los procesos.
 * @return int 
 */
int main(int argc, char *argv[])
{
    int socket = conectar(argv[1]);
    sesionActiva(socket, argv[1], argv[0]);
    close(socket);
    return 0;
}

/**
 * @brief Se crea el socket del cliente e intenta la conexion con el socket
 *        del servidor. En caso de no poder conectarse, espera 5 segundos
 *        para volver a intentarlo.
 * 
 * @param sock_name socket UNIX que usa el cliente y servidor para comunicarse
 * @return int identificador (file descriptor) del cliente para comunicarse
 *         con el servidor
 */
int conectar(char *sock_name)
{
    int sockfd, servlen;
    uint8_t conexion = 1;
    struct sockaddr_un serv_addr;
    char buffer[20];
    /* Inicialización del socket */
    memset((char *)&serv_addr, '\0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;        /* Tipo de socket */
    strcpy(serv_addr.sun_path, sock_name); /* Directorio del socket UNIX, pasado como argumento */
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0); /* Socket UNIX TCP */

    if (sockfd < 0)
    {
        perror("creación de socket");
        exit(1);
    }
    while (conexion)
    {
        printf("\n=====================================");
        if (connect(sockfd, (struct sockaddr *)&serv_addr, servlen) < 0)
        {
            printf("\n  Cliente inicializado - Intento[%d] \n", conexion);
            printf("  Conexion [");
            printf(ANSI_COLOR_RED "x");
            printf(ANSI_COLOR_RESET "]");
            printf("\n=====================================\n");
            conexion += 1;
            sleep(5);
            //exit(1);
        }
        else
        {
            printf("\n  Cliente inicializado [ID: %d] \n", getpid());
            sprintf(buffer, "%d", getpid());
            write(sockfd, buffer, sizeof(buffer));
            memset(buffer, '\0', TAM);
            getfirmware_version(buffer);
            printf("  %s\n", buffer);
            printf("  Conexion [");
            printf(ANSI_COLOR_GREEN "√");
            printf(ANSI_COLOR_RESET "]");
            printf("\n=====================================\n");
            conexion = 0;
        }
    }

    /* Posterior a la conexion con el servidor, se verifica si existe el archivo
       cliente2 (de actualizacion de firmware). Si existe lo elimina. */
    FILE *fd = fopen("cliente2", "r");
    if (fd != NULL)
    {
        fclose(fd);
        remove("cliente2");
    }
    return sockfd;
}

/**
 * @brief Mantiene la sesion hasta que el servidor finalice la sesion empleando
 *        el comando sat_logoff. Se mantiene a la espera de que el servidor
 *        envie un comando, lo comprueba y actua en consecuencia.
 * 
 * @param socket file descriptor del socket cliente
 * @param sock_name socket UNIX empleado para la comunicacion entre cliente
 *                  y servidor
 * @param nombre nombre del codigo ejecutable
 */
void sesionActiva(int socket, char *sock_name, char *nombre)
{
    char buffer[TAM];
    int n = 0;
    int sesionActiva = 1;
    memset(buffer, '\0', sizeof(buffer));

    while (sesionActiva)
    {
        printf("Satelite Activo...\n");

        memset(buffer, '\0', sizeof(buffer));
        n = read(socket, buffer, sizeof(buffer)); //Leo el comando enviado por el servidor
        if (n < 0)
        { //y lo voy comparando
            perror("lectura de socket");
            exit(1);
        }
        if (!strcmp(buffer, "update_firmware"))
        {
            update_Firmware(socket, nombre, sock_name);
            sesionActiva = 0;
        }
        if (!strcmp(buffer, "start_scanning"))
        {
            n = start_Scanning(socket);
        }
        if (!strcmp(buffer, "obtener_telemetria"))
        {
            n = obtener_Telemetria(socket, sock_name);
        }
        if (!strcmp(buffer, "sat_logoff"))
        {
            printf(ANSI_COLOR_RED);
            printf("\nCerrando comunicacion.\n");
            printf(ANSI_COLOR_RESET);
            close(socket);
            exit(0);
        }
    } //Fin while sesion activa
}

/**
 * @brief Actualiza la versión del sistema. Recibe la cantidad de bytes del 
 *        nuevo binario y luego comienza a recibirlo y generar el nuevo archivo. 
 *        Una vez completada la descarga del nuevo ejecutable, sobreecribe el 
 *        proceso actual en ejecución y reconecta con el servidor levantando
 *        ya la nueva version.
 * 
 * @param sock interfaz socket (id)
 * @param nombre nombre del programa en ejecucion
 * @param sock_name nombre del socket file descriptor
 */
void update_Firmware(int sock, char *nombre, char *sock_name)
{
    printf("=====================================\n\n");
    printf("UPDATE FIRMWARE\n\n");
    char buffer[TAM];
    char old_name[10], new_name[10];
    int new_exe;
    long byteRead = 0;
    int npackages = 0;

    /* Renombro al ejecutable actual para receptar el nuevo 
       ejecutable actualizado */
    strtok(nombre, "/");
    strcpy(old_name, strtok(NULL, " "));

    strcpy(new_name, old_name);
    strcat(new_name, "2");

    rename(old_name, new_name);

    if ((new_exe = open(old_name, O_WRONLY | O_CREAT | O_TRUNC, 0777)) < 0)
    {
        printf("Error creando el file\n");
        return;
    }

    write(sock, "DONE", 4);
    memset(buffer, '\0', sizeof(buffer));

    if ((byteRead = read(sock, buffer, sizeof(buffer)) != 0))
    {
        if (byteRead <= 0)
        {
            perror("ERROR leyendo del socket");
        }
    }

    printf("N° de paquetes a recibir: %s\n", buffer);
    npackages = atoi(buffer);

    for (int i = 0; i < npackages; i++)
    {
        memset(buffer, '\0', sizeof(buffer));
        if ((byteRead = read(sock, buffer, sizeof(buffer))) != 0)
        {
            if (byteRead <= 0)
            {
                perror("ERROR leyendo del socket");
                continue;
            }
        }
        if ((write(new_exe, buffer, (size_t)byteRead) < 0))
        {
            perror("ERROR escribiendo en el file");
            exit(EXIT_FAILURE);
        }
    }
    memset(buffer, '\0', sizeof(buffer));

    close(sock);
    printf("Reiniciando...\n");
    printf("=====================================\n");
    close(new_exe);
    sleep(2);

    /* Prepara la ejecucion del nuevo firmware */
    memset(buffer, '\0', TAM);
    strcpy(buffer, "./");
    strcat(buffer, old_name);

    /* socket UNIX ../server, debe ser tomado como parametro...VER */
    chmod(old_name, S_IRWXO | S_IRWXU | S_IRWXG);
    char *args[] = {buffer, sock_name, NULL};
    execvp(args[0], args);
}

/**
 * @brief Envia imagen satelital. Determina la cantidad de bytes de la
 *        imagen y luego obtiene la cantidad paquetes a ser enviados
 *        de tal manera que el protocolo TCP no lo fragmente.     
 * 
 * @param socket 
 * @return int 
 */
int start_Scanning(int socket)
{
    int send_img = 0;
    int packages = 0;
    struct stat buf;
    int count;
    char sendBuffer[FILE_BUFFER_SIZE];

    printf("=====================================\n\n");
    printf("START SCANNING\n\n");

    if ((send_img = open("geoes.jpg", O_RDONLY)) < 0)
    {
        printf("No existe la imagen\n");
        return 0;
    }

    memset(&packages, '\0', sizeof(packages));
    memset(sendBuffer, '\0', sizeof(sendBuffer));
    fstat(send_img, &buf);
    off_t fileSize = buf.st_size;
    printf("Tamaño de Imagen: %li\n", fileSize);
    packages = fileSize / sizeof(sendBuffer);
    printf("N° de paquetes a enviar : %i\n", packages);
    memset(sendBuffer, '\0', sizeof(sendBuffer));

    if (send(socket, &packages, sizeof(int), 0) < 0)
    {
        perror("ERROR enviando");
    }

    while ((count = (int)read(send_img, sendBuffer, sizeof(sendBuffer))) > 0)
    {
        if (send(socket, sendBuffer, count, 0) < 0)
        {
            perror("ERROR enviando");
        }
        memset(sendBuffer, '\0', sizeof(sendBuffer));
    }
    close(send_img);
    write(socket, sendBuffer, count);
    printf("Finalizado envio de Imagen\n");
    printf("\n=====================================\n");
    memset(sendBuffer, '\0', sizeof(sendBuffer));
    return 1;
}

/**
 * @brief Obtiene información relevante del sistema y lo envía al
 *        servidor mediante socket DATAGRAM.
 * 
 * @param socketfd 
 * @param sock_name 
 * @return int 
 */
int obtener_Telemetria(int socketfd, char *sock_name)
{
    char sock_name_UDP[20];
    memset(sock_name_UDP, '\0', sizeof(sock_name_UDP));
    strcpy(sock_name_UDP, sock_name);
    strcat(sock_name_UDP, "_UDP");
    printf("=====================================\n\n");
    printf("ENVIANDO TELEMETRIA\n\n");

    long minuto = 60;        //Variables usadas para acomodar el formato de
    long hora = minuto * 60; //salida del uptime
    long dia = hora * 24;    //probar con "define" desde las cabezeras
                             //para no desperdiciar memoria.

    char buffer[TAM2];
    struct sysinfo estructuraInformacion;
    memset(buffer, '\0', TAM2);
    sysinfo(&estructuraInformacion); //Obtengo datos del sistema
    int descriptor_socket, resultado;
    struct sockaddr_un struct_cliente;
    /* Creacion de socket */
    if ((descriptor_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
    }
    /* Inicialización y establecimiento de la estructura del cliente */
    memset(&struct_cliente, 0, sizeof(struct_cliente));
    struct_cliente.sun_family = AF_UNIX;
    strncpy(struct_cliente.sun_path, sock_name_UDP, sizeof(struct_cliente.sun_path));
    long tiempo = estructuraInformacion.uptime;

    for (int i = 0; i < 7; i++)
    {
        memset(buffer, '\0', TAM2);
        switch (i)
        {
        case 0:
            sprintf(buffer, "ID satelite: %d", getpid()); //Tomo como id del satelite al pid del proceso actual
            break;
        case 1:
            getfirmware_version(buffer);
            break;
        case 2:
            sprintf(buffer, "Uptime : %ld dias, %ld:%02ld:%02ld",
                    tiempo / dia, (tiempo % dia) / hora,
                    (tiempo % hora) / minuto, tiempo % minuto);
            break;
        case 3:
            bootTime(buffer);
            break;
        case 4:
            hostname(buffer);
            break;
        case 5:
            memoria(buffer);
            break;
        case 6:
            CPU(buffer);
            break;
        default:
            break;
        }
        sleep(1);
        /* Envío de datagrama al servidor */
        resultado = sendto(descriptor_socket, buffer, TAM2, 0, (struct sockaddr *)&struct_cliente, sizeof(struct_cliente));
        if (resultado < 0)
        {
            perror("sendto");
            exit(1);
        }
        printf("[%d-7] %s\n", i + 1, buffer);
    } //fin for
    //finaliza socket sin conexion
    printf("\n=====================================\n");
    close(descriptor_socket);
    return 0;
}

/**
 * @brief Get the Value object
 * 
 * @param file 
 * @param value 
 * @param key 
 */
void getValue(char *file, char *value, char *key)
{
    char buffer[500];
    char *match = NULL;
    FILE *fd;
    fd = fopen(file, "r");

    while (feof(fd) == 0)
    {
        fgets(buffer, 500, fd);
        match = strstr(buffer, key);
        if (match != NULL)
            break;
    }

    fclose(fd);
    strcpy(value, match);
    return;
}

/**
 * @brief Get the firmware version object
 * 
 * @param buffer 
 */
void getfirmware_version(char *buffer)
{
    strcat(buffer, "Version Firmware: ");
    strcat(buffer, "1");
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void hostname(char *buffer)
{
    char name[30];
    FILE *fd;
    fd = fopen("/proc/sys/kernel/hostname", "r");

    fscanf(fd, "%[^\n]s", name); //[^x] hasta que se encuentre x.
    strcat(buffer, "Hostname: ");
    strcat(buffer, name);
    fclose(fd);
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void CPU(char *buffer)
{
    char cpu[10];
    FILE *fp;
    char *command = "grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage}'";
    fp = popen(command, "r");
    fscanf(fp, "%s", cpu);
    strcat(buffer, "CPU: ");
    strcat(buffer, cpu);
    strcat(buffer, "%");
    fclose(fp);
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void uptime(char *buffer)
{
    FILE *fd;
    float time;
    char hms[18], str[10];
    int hora, minuto, segundo;
    fd = fopen("/proc/uptime", "r");
    fscanf(fd, "%f", &time);

    hora = (int)time / HORA;
    segundo = (int)time % HORA;
    minuto = (int)segundo / MIN;
    segundo %= MIN;

    strcat(buffer, "Uptime: ");
    sprintf(str, "%d", hora);
    strcpy(hms, str);
    strcat(hms, ":");
    sprintf(str, "%d", minuto);
    strcat(hms, str);
    strcat(hms, ":");
    sprintf(str, "%d", segundo);
    strcat(hms, str);
    strcat(buffer, hms);
    strcat(buffer, "\n");
    fclose(fd);
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void bootTime(char *buffer)
{
    char value[SIZE];
    time_t btime;
    unsigned int aux;
    char booted[40];

    getValue("/proc/stat", value, "btime");
    sscanf(value, "btime %u", &aux);
    btime = (time_t)aux;
    strftime(booted, sizeof(booted), "%c", localtime(&btime));
    strcat(buffer, "Boot Time: ");
    strcat(buffer, booted);
}

/**
 * @brief 
 * 
 * @param buffer 
 */
void memoria(char *buffer)
{
    char value[SIZE], str[10];
    unsigned int memTotal, memFree;

    getValue("/proc/meminfo", value, "MemTotal");
    sscanf(value, "MemTotal: %u", &memTotal);
    getValue("/proc/meminfo", value, "MemFree");
    sscanf(value, "MemFree: %u", &memFree);
    strcat(buffer, "MemTotal: ");
    sprintf(str, "%d", (memTotal / 1024));
    strcat(buffer, str);
    strcat(buffer, " - ");
    strcat(buffer, "MemFree: ");
    sprintf(str, "%d", (memFree / 1024));
    strcat(buffer, str);
}
