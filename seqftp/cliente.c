#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFF_SIZE 100

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

//ENVIO LOS DATOS AL SERVIDOR
void send_msg(int sd, char *oper, char *param){
	
	char buff[BUFF_SIZE];

	if(param != NULL){
		sprintf(buff, "%s %s\r\n",oper,param);
	}
	else{
		sprintf(buff, "%s\r\n",oper);
	}

	//debo enviar el comando al servidor
	if(write(sd,buff,sizeof(buff)) == -1){
		printf("Error: no se pudo enviar el mensaje dentro de la funcion send_msg().\n");
                exit(-1);
	}

	memset(buff,0,sizeof(buff));
}

//LEO LO QUE EL CLIENTE INGRESE POR TECLADO
char* read_input(){

	char *input = malloc(BUFF_SIZE);
	if(fgets(input,BUFF_SIZE,stdin)){
		return strtok(input,"\n");
	}
	return NULL;
}


void authenticate(int sd){
	
	char *input;
	char buf[BUFF_SIZE];

	//ENVIO EL NOMBRE DEL USUARIO AL SERVIDOR
	printf("Nombre de usuario: ");
	input = read_input();
	
	send_msg(sd,"USER",input);

	free(input);
	
	//LEO LA RESPUESTA DEL SERVIDOR
	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

        printf("Mensaje del servidor: %s\n",buf);
	
	memset(buf,0,sizeof(buf));

	//ENVIO LA CONTRASEÑA AL SERVIDOR
	printf("Contraseña: ");
	input = read_input();

	send_msg(sd,"PASS",input);

	free(input);

	//LEEMOS LA RESPUESTA DEL SERVIDOR TRAS ANALIZAR NUESTRO USUARIO Y CONTRASEÑA
	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

        printf("Mensaje del servidor: %s\n",buf);

	memset(buf,0,sizeof(buf));
}

int main(int argc, char *argv[]){

	int sd;
	int puerto;
	char buf[BUFF_SIZE];
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
	
	//EL USUARIO DEBE LOGUEARSE
	authenticate(sd);

	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE
	//close(sd);

	return 0;


}







