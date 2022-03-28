#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MSG_220 "220 srvftp version 1.0.\n"

int main(int argc, char *argv[]){

	int sd;	 // descriptor del socket
	int sd2; // descriptor para hablar con el cliente
	int puerto;
	unsigned int length;
    	char buf[50];

	struct sockaddr_in addr;

	//VERIFICAMOS QUE SE HAYA AGREGADO UN SOLO ARGUMENTO
	if(argc != 2){
		printf("Cantidad de argumentos invalida.\n");
		exit(-1);
	}

	puerto = atoi(argv[1]);

	//ASIGNAMOS PUERTO E IP AL SOCKET
	addr.sin_family = AF_INET;
	addr.sin_port = htons(puerto);
	addr.sin_addr.s_addr = INADDR_ANY;

	//SE ABRE EL SOCKET
	sd = socket(AF_INET,SOCK_STREAM,0);

	if(sd < 0){
		printf("Error de apertura del socket.\n");
		exit(-1);
	}
	
	//AVISAMOS AL S.O. QUE ABRIMOS UN SOCKET Y QUE DEBE ASOCIAR NUESTRO PROGRAMA A ESE SOCKET
	if(bind(sd,(struct sockaddr*)&addr,sizeof(addr)) == -1){
		printf("Error de asociacion de socket (error en bind()).\n");
		exit(-1);
	}

	//ESCUCHAMOS LAS PETICIONES QUE LLEGUEN DEL LADO DEL CLIENTE
	if(listen(sd,1) == -1){
		printf("No se ha podido levantar la atencion.\n");
		exit(-1);
	}

	//RECIBIMOS A LOS CLIENTES QUE ENTREN EN LA LLAMADA

	length = sizeof(addr);
	sd2 = accept(sd,(struct sockaddr*)&addr,(socklen_t*)&length);
	if(sd2 == -1){
		printf("Error de conexion.\n");
		exit(-1);
	}

	//ENVIAMOS UN MENSAJE DE SALUDO AL CLIENTE
    	
	//escribimos un mensaje en el fichero del cliente
    	if(write(sd2,MSG_220,sizeof(MSG_220)) == -1){
        	printf("Error: no se pudo enviar el mensaje.\n");
	        exit(-1);
    	}

	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE Y LUEGO DEL SERVIDOR
    	close(sd2);
    	close(sd);

	return 0;

}
