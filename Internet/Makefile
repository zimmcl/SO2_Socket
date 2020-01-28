.DELETE_ON_ERROR:#Borra el target en caso de producirse un error en alguno de los comandos

#Definimos las macros
CC=gcc	#Compilador a usar
CFLAGS= -std=gnu99 -Werror -Wall -pedantic -fno-stack-protector	#Banderas a utilizar

all: cliente cliente2 servidor
	@echo "Compilación exitosa | ${shell date --iso=seconds}"
	@cp cliente ./Cliente1
	@cp ./imagen/geoes.jpg ./Cliente1


cliente: cliente.c
	${CC} ${CFLAGS} -o cliente cliente.c
	@rm -f cliente.o

servidor: servidor.c
	${CC} ${CFLAGS} -o servidor servidor.c
	@rm -f servidor.o	

cliente2: cliente2.c
	${CC} ${CFLAGS} -o cliente2 cliente2.c
	@rm -f cliente2.o

clean:
	@rm -f cliente cliente2 servidor
	@rm -f ./Cliente1/cliente
	@rm -f ./Cliente1/geoes.jpg
	@echo "Se eliminaron correctamente todos los archivos."
