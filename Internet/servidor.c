
/**
 * @file servidor.c
 * @author Ezequiel Zimmel (ezequielzimmel@gmail.com)
 * @brief Implemetacion de socket INET. El servidor funciona como estacion terrestre
 *        solicitando datos a satelite.  
 *        Comienza creando un socket INET orientado a la conexión. El usuario debe ingresar
 *        por linea de comandos solo su nombre (login <usuario>), la direccion y el puerto
 *        del servidor es fijo,192.168.1.5:6020.  
 *        Una vez realizada la validacion de credenciales se crea el socket y queda a la
 *        espera de una conexion entrante por parte de un satelite. Cuando conecta, deriva
 *        la conexion original a una conexion secundaria, proceso hijo, para mantener al
 *        proceso padre a la espera de nuevas conexiones.
 * @version 0.1
 * @date 2020-01-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */

/* Librerias usados por los distintos codigos fuente */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define TAM 80
#define TAM2 150
#define BUFSIZE 1024
#define DIRECTORIO_IMAGEN "/imagen"
#define BYTES_STREAM 1500
#define BUFF_SIZE 1024
#define FILE_BUFFER_SIZE 1500
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_RESET "\x1b[0m"

/* Funciones que escribí */
int validacion(char *, char *);
void sesion(int, char *, char *, char *);
int update_Firmware(int);
int start_Scanning(int);
int obtener_Telemetria(int, char *, char *);
int Servidor_UP(char *, char *);

/**
 * @brief Estado inicial de conexion al servidor. Realiza la validacion de las
 *        credenciales ingresadas. Si no son reconocidas se solician nuevamente.
 *        Cuando se validan, inicializa el servicio de conexion con el satelite
 *        mediante la funcion Servidor_UP. 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char *argv[])
{
    int conexion = 0;
    char bufferConexion[30];
    char usuario[20], ip[INET_ADDRSTRLEN], port[5];

    printf("\nInicio del programa Servidor");
    printf("\n===========================\n");

    do
    {
        memset(&bufferConexion[0], 0, sizeof(bufferConexion));
        printf("desconectado");
        printf("~$ ");
        fgets(bufferConexion, TAM - 1, stdin);
        conexion = validacion(bufferConexion, usuario);
    } while (conexion == 0);

    printf("Bienvenido ");
    printf(ANSI_COLOR_BLUE);
    printf("%s\n", usuario);
    printf(ANSI_COLOR_GREEN);
    printf(ANSI_COLOR_GREEN);
    printf("Esperando por conexión entrante\n");
    printf(ANSI_COLOR_RESET);
    int socket = Servidor_UP(ip, port);
    sesion(socket, usuario, ip, port);

    return 0;
}

/**
 * @brief Crea el socket para atender las peticiones entrantes.
 *        Cuando se conecta un cliente, deriva dicha conexion a un proceso
 *        hijo para mantenerse a le espera de nuevas conexiones entrantes.
 * 
 * @param ip 
 * @param port 
 * @return int 
 */
int Servidor_UP(char *ip, char *port)
{
    int sockfd, newsockfd, pid;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;
    char str2[INET_ADDRSTRLEN];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("creación de  socket");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("192.168.1.5");
    //serv_addr.sin_addr.s_addr = inet_addr("192.168.0.10");
    serv_addr.sin_port = htons(6020);
    inet_ntop(AF_INET, &(serv_addr.sin_addr.s_addr), str2, INET_ADDRSTRLEN);

    strcpy(ip, str2);
    sprintf(port, "%d", ntohs(serv_addr.sin_port));

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ligadura");
        exit(1);
    }

    printf("Proceso: %d - socket disponible: %s:%d\n", getpid(), str2, ntohs(serv_addr.sin_port));

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("accept");
            exit(1);
        }

        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(1);
        }

        if (pid == 0)
        { //proceso hijo
            //close( sockfd );
            return (newsockfd);
        }
        else
        {
            char buffer[TAM];
            memset(buffer, '\0', sizeof(buffer));
            read(newsockfd, buffer, sizeof(buffer) - 1);
            printf(ANSI_COLOR_GREEN);
            printf("\nSERVIDOR: Nuevo cliente (PID: %s) conectado desde %s:%d\n", buffer, inet_ntoa(cli_addr.sin_addr), htons(cli_addr.sin_port));
            printf(ANSI_COLOR_RESET);
        }
    } //Fin while(1)
    close(sockfd);
    return 0;
}

/**
 * @brief Valida las credecinales del usuario que intenta logearse. Las contrasenas 
 *        se encuentran almacenadas en un archivo de texto. Durante el ingreso de la
 *        contrasena se ocultan los caracteres. Si al cabo de 3 intentos las credenciales
 *        no son validadas, da por finalizada la sesion de logeo solicitando nuevamente el 
 *        login.
 * 
 * @param buffer 
 * @param usuario 
 * @return int 
 */
int validacion(char *buffer, char *usuario)
{
    int intentos = 4;
    struct termios term, term_orig;
    tcgetattr(STDIN_FILENO, &term);
    term_orig = term;
    term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    char *contras = (char *)malloc(10);
    char *token, *command, *pass;
    char line[256];
    buffer[strlen(buffer) - 1] = 0;

    printf("Ingrese contraseña: ");

    fgets(contras, TAM - 1, stdin);
    contras[strlen(contras) - 1] = 0;

    /* Remember to set back, or your commands won't echo! */
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);

    token = strtok(buffer, " ");
    command = token;
    if (strcmp(command, "login") != 0)
    {
        printf("\nPara loguearse utilice: login <usuario> \n");
        return 0;
    }

    strcpy(usuario, strtok(NULL, " "));

    FILE *fp = fopen("archivos/usuarios.txt", "r");
    if (fp == 0)
    {
        perror("Canot open input file\n");
        exit(-1);
    }
    else
    {
        while (fgets(line, sizeof(line), fp))
        {
            token = strtok(line, ":");
            if (strcmp(token, usuario) == 0)
            {
                token = strtok(NULL, " ");
                pass = token;
                while (intentos > 1)
                {
                    if (strcmp(pass, contras) == 0)
                    {
                        printf("\r                             ");
                        free(contras);
                        fclose(fp);
                        printf("\rIngrese contraseña: [");
                        printf(ANSI_COLOR_GREEN);
                        printf("√");
                        printf(ANSI_COLOR_RESET);
                        printf("]\n");
                        return 1;
                    }
                    else
                    {
                        intentos--;
                        tcgetattr(STDIN_FILENO, &term);
                        term_orig = term;
                        term.c_lflag &= ~ECHO;
                        tcsetattr(STDIN_FILENO, TCSANOW, &term);

                        printf("\r[%d]Ingrese contraseña: [", intentos);
                        printf(ANSI_COLOR_RED);
                        printf("x");
                        printf(ANSI_COLOR_RESET);
                        printf("] ");
                        fgets(contras, TAM - 1, stdin);
                        contras[strlen(contras) - 1] = 0;

                        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
                        token = strtok(buffer, " ");
                        command = token;
                    }
                }
            }
        }
    }
    printf("ERROR... \n");
    fclose(fp);
    free(contras);
    return 0;
}

/**
 * @brief Mantiene la sesion para comunicarse con el satelite. Cada comando ingresado por el 
 *        usuario es analizado y si es valido activa el procedimiento, en caso contrario
 *        descarta el comando.
 * 
 * @param socket 
 * @param usuario 
 * @param ip 
 * @param port 
 */
void sesion(int socket, char *usuario, char *ip, char *port)
{
    int n = 0;
    char comando[50];
    int sesionActiva = 1;
    printf(ANSI_COLOR_RESET);
    printf("\nEscriba 'opciones' para listar los comandos disponibles.\n");

    while (sesionActiva)
    {
        printf(ANSI_COLOR_CYAN "%s", usuario);
        printf(ANSI_COLOR_RESET);
        printf("@%s:%s # ", ip, port);

        memset(comando, '\0', 50);
        scanf("%s", comando);

        if (!strcmp(comando, "update_firmware"))
        {
            printf("Enviando orden UPDATE FIRMWARE\n");
            n = update_Firmware(socket);
            strcpy(comando, "sat_logoff");
            n = n;
        }

        if (!strcmp(comando, "start_scanning"))
        {
            printf("Enviando orden START SCANNING\n");
            n = start_Scanning(socket);
        }

        if (!strcmp(comando, "obtener_telemetria"))
        {
            printf("Enviando orden OBTENER TELEMETRIA\n");
            n = obtener_Telemetria(socket, ip, port);
        }
        if (!strcmp(comando, "opciones"))
        {
            printf(ANSI_COLOR_RESET "\n%-20sOPCIONES\n", " ");
            printf(" 1)update_firmware\n"
                   " 2)start_scanning \n"
                   " 3)obtener_telemetria \n"
                   " 4)opciones \n"
                   " 5)sat_logoff \n\n");
        }
        if (!strcmp(comando, "sat_logoff"))
        {
            printf("Cerrando comunicacion con cliente.\n");
            printf(ANSI_COLOR_GREEN);
            printf("Esperando por conexión entrante\n");
            printf(ANSI_COLOR_RESET);

            int n = write(socket, "sat_logoff", 12);
            if (n < 0)
            {
                perror("escritura en socket");
                exit(1);
            }
            close(socket);
            exit(0);
        }
    } //Fin while sesion activa
}

/**
 * @brief Procedimiento de actualizacion del binario del satelite.
 * 
 * @param sock 
 * @return int 
 */
int update_Firmware(int sock)
{
    printf("=====================================\n\n");
    printf("UPDATE FIRMWARE\n\n");

    char buffer[TAM];
    int new_exe;
    struct stat buf;
    int count;

    memset(buffer, '\0', sizeof(buffer));
    strcpy(buffer, "update_firmware");
    write(sock, buffer, sizeof(buffer));

    if ((new_exe = open("cliente2", O_RDONLY)) < 0)
    {
        printf("No existe el update de firmware solicitado\n");
        return 0;
    }

    fstat(new_exe, &buf);
    off_t fileSize = buf.st_size;
    printf("Tamaño del binario: %li\n", fileSize);
    int packages = fileSize / sizeof(buffer);
    printf("N° de paquetes a enviar: %d\n", packages);

    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, 4);
    sprintf(buffer, "%d", packages);

    if (write(sock, buffer, sizeof(buffer)) < 0)
    {
        perror("ERROR enviando");
    }

    while ((count = (int)read(new_exe, buffer, sizeof(buffer))) > 0)
    {
        usleep(1000);
        if (write(sock, buffer, (size_t)count) < 0)
        {
            perror("ERROR enviando");
        }
        memset(buffer, '\0', sizeof(buffer));
    }
    close(new_exe);
    printf("=====================================\n\n");
    return 1;
}

/**
 * @brief Procedimiento que recepta la imagen geoterrestre que envia
 *        el satelite.
 * 
 * @param socket 
 * @return int 
 */
int start_Scanning(int socket)
{
    printf("=====================================\n\n");
    printf("START SCANNING\n\n");

    int n = write(socket, "start_scanning", 14); //Envia el comando ingresado al cliente
    if (n < 0)
    { //para que sepa que funcion ejecutar.
        perror("escritura en socket");
        exit(1);
    }

    int new_img = 0;
    remove("c1.jpg");
    if ((new_img = open("c1.jpg", O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
    {
        printf("Error creando el file\n");
        return 0;
    }
    char recvBuffer[FILE_BUFFER_SIZE];
    long byteRead = 0;
    int npackages = 0;
    memset(&npackages, '\0', sizeof(npackages));
    memset(recvBuffer, '\0', sizeof(recvBuffer));

    if ((byteRead = read(socket, &npackages, sizeof(int))) != 0)
    {
        if (byteRead <= 0)
        {
            perror("ERROR leyendo del socket");
        }
    }

    float porcentaje;
    printf("N° de paquetes a recibir: %i\n", npackages);
    for (int i = 0; i <= npackages; i++)
    {
        porcentaje = ((float)i / (float)npackages) * 100;
        printf("\r[%i - %i] [%.0f%%]", i, npackages, porcentaje);
        usleep(100);
        memset(recvBuffer, '\0', sizeof(recvBuffer));
        if ((byteRead = read(socket, recvBuffer, sizeof(recvBuffer))) != 0)
        {
            if (byteRead <= 0)
            {
                perror("ERROR leyendo del socket");
                continue;
            }
        }
        if ((write(new_img, recvBuffer, (size_t)byteRead) < 0))
        {
            perror("ERROR escribiendo en el file");
            exit(EXIT_FAILURE);
        }
    }
    close(new_img);
    printf(" Finalizada la recepcion de Imagen\n");
    printf("=====================================\n\n");
    return 1;
}

/**
 * @brief Procedimiento que obtiene datos de estado del satelite.
 *        La comunicacion se realiza a traves de socket UDP, no orientado
 *        a la conexión. El puerto a emplear es el mismo que el puerto de
 *        la conexion TCP.
 * 
 * @param socketfd 
 * @param ip 
 * @param port 
 * @return int 
 */
int obtener_Telemetria(int socketfd, char *ip, char *port)
{
    int n = write(socketfd, "obtener_telemetria", 18); //Envio el comando ingresado al cliente
    if (n < 0)
    { //para que sepa que funcion ejecutar
        perror("escritura en socket");
        exit(1);
    }

    int sockfd_udp, puerto;
    socklen_t tamano_direccion;
    char buffer[TAM2];
    struct sockaddr_in serv_addr;

    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_udp < 0)
    {
        perror("ERROR en apertura de socket");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    puerto = atoi(port);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(puerto);
    memset(&(serv_addr.sin_zero), '\0', 8);

    if (bind(sockfd_udp, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR en binding");
        exit(1);
    }
    //Envio al cliente el numero de puerto UDP
    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%d", ntohs(serv_addr.sin_port));

    n = write(socketfd, buffer, sizeof(buffer));
    if (n < 0)
    {
        perror("escritura en socket");
        exit(1);
    }

    printf("Usando socket: %s:%d\n", ip, ntohs(serv_addr.sin_port));

    tamano_direccion = sizeof(struct sockaddr);

    printf("=====================================\n\n");
    printf("OBTENER TELEMETRIA\n\n");
    for (int i = 0; i < 7; i++)
    {
        n = recvfrom(sockfd_udp, (void *)buffer, TAM, 0, (struct sockaddr *)&serv_addr, &tamano_direccion);
        if (n < 0)
        {
            perror("recepción");
            exit(1);
        }
        printf("[%d-7] %s\n", i + 1, buffer);
        memset(buffer, '\0', sizeof(buffer));
    }
    printf("\n=====================================\n\n");
    close(sockfd_udp);
    memset(buffer, '\0', sizeof(buffer));
    return 0;
}
