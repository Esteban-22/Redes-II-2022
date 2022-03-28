#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

//Funcion que se llama en validar_ip()
int validar_nro(char *str){
	while(*str){
		if(!isdigit(*str)){ //si el char no es un nro, retorna 'false'
			return 0;
		}
		str++;
	}
	return 1;
}

//Funcion que chequea si la IP tiene un formato valido
int validar_ip(char *ip){
	int num, punto = 0;
	char *ptr;
	if(ip == NULL){
		return 0;
	}
	ptr = strtok(ip,"."); //corta el string si encuentra un punto
	if(ptr == NULL){
		return 0;
	}
	while(ptr){
		if(!validar_nro(ptr)){	//chequea si los nros entre puntos son validos
			return 0;
		}
		num = atoi(ptr);	//convierte el string a un nro
		if(num >= 0 && num <= 255){
			ptr = strtok(NULL,"."); //
			if(ptr != NULL){
				punto++;	// incrementa el contador de puntos
			}
		} else {
			return 0;
		}
	}
	if(punto != 3){
		return 0;	//
	}
	return 1;
}


int main(int argc, char *argv[]){

	int sd;
	int puerto;
	char buf[50];
	struct sockaddr_in servidor;

	//VERIFICAMOS QUE SE HAYAN AGREGADO DOS ARGUMENTOS
	if(argc != 3){
		printf("Cantidad de argumentos invalida.\n");
		exit(-1);
	}
	
	char ip[16];
	memset(ip,'\0',sizeof(ip));       
	strcpy(ip,argv[1]);

	//Validamos el formato de la direccion IP (v4)
    	validar_ip(ip);

	//Guardamos el puerto en una variable
	puerto = atoi(argv[2]);

	//OBTENEMOS LOS DATOS DEL SERVIDOR
 	servidor.sin_family = AF_INET;
   	servidor.sin_port = htons(puerto);
    	servidor.sin_addr.s_addr = inet_addr(argv[1]);

	//ABRO DEL SOCKET
	if((sd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		printf("Error de apertura del socket.\n");
		exit(-1);
	}

	//NOS CONECTAMOS AL SERVIDOR
	if(connect(sd,(struct sockaddr*)&servidor,sizeof(servidor)) == -1){
		printf("Error: no se pudo conectar con el servidor.\n");
		exit(-1);
	}

    	//LEEMOS EL MENSAJE EN EL FICHERO Y LO COPIAMOS EN EL BUFFER
    	if(read(sd,buf,sizeof(buf)) == -1){
        	printf("Error: no se pudo leer el mensaje del servidor.\n");
        	exit(-1);
   	}

    	printf("Mensaje del servidor: %s\n",buf);

	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE
	close(sd);

	return 0;


}







