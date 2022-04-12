#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFF_SIZE 100
#define BYTES 512

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
	int num, dot = 0;
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
				dot++;	// incrementa el contador de puntos
			}
		} else {
			return 0;
		}
	}
	if(dot != 3){
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
	char *token = NULL;

	bool confirm_log = false;

	while(confirm_log == false){
	
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
		
		token = strtok(buf, " ");

		if(strcmp(token,"530") != 0){
			confirm_log = true;
		}

		memset(buf,0,sizeof(buf));

	}

}


void get(int sd, char *file_name){

	FILE *file;
	char buf[1024];
	char *temp = NULL;
	int r;

	//ENVIAMOS EL COMANDO 'RETR' AL SERVIDOR
	send_msg(sd,"RETR",file_name);		//file_name es el parametro (el nombre del archivo)

	//RECIBIMOS LA RESPUESTA DEL SERVIDOR QUE CONTIENE EL TAMAÑO DEL ARCHIVO
	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

        printf("Mensaje del servidor: %s\n",buf);
	
        memset(buf,0,sizeof(buf));
	
	//RECIBIMOS EL ARCHIVO
	file = fopen(file_name, "w");

	//VOLCAMOS EL CONTENIDO EN EL ARCHIVO ABIERTO QUE SE ENCUENTRA EN EL DIRECTORIO DEL CLIENTE
	if(read(sd,buf,sizeof(buf)) == -1){
               	printf("Error: no se pudo leer el mensaje del servidor.\n");
               	exit(-1);
        }
	
	//para evitar que llene el archivo de 512 bytes en cada pasada (aun si no llega a esa cantidad) usamos un puntero temporal, donde copiamos en él el contenido del buffer hasta que no encuentra nada mas, y luego escribimos en el archivo
	
	temp = strtok(buf," ");
	fwrite(temp,1,strlen(temp),file);
	
	memset(buf,0,sizeof(buf));
	temp = NULL;

	//IMPRIMIMOS LA CONFIRMACION DE TRANSFERENCIA COMPLETA
	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

	printf("\nMensaje del servidor: %s\n\n",buf);
	memset(buf,0,sizeof(buf));

	fclose(file);
	
}


void quit(int sd){
	
	char buf[BUFF_SIZE];

	send_msg(sd,"QUIT",NULL);

	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }
	
	printf("Mensaje del servidor: %s\n",buf);

	memset(buf,0,sizeof(buf));
}


void operate(int sd){

	char *input, *oper, *param;

	while(true){

		printf("Ingrese una operacion ('get file_name.txt' | 'quit'): ");

		input = read_input();

		if(input == NULL){
			continue;
		}

		oper = strtok(input, " ");

		if(strcmp(oper,"get") == 0){
			param = strtok(NULL, " ");
			
			get(sd,param);
		
		}
		else if(strcmp(oper,"quit") == 0){
			
			quit(sd);
			
			break;
		}
		else{
			printf("Unexpected command.\n\n");
		}
		free(input);
	}
	free(input);

}

int main(int argc, char *argv[]){

	int sd;
	int port;
	char buf[BUFF_SIZE];
	struct sockaddr_in server;
	struct sockaddr_in client;

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
	port = atoi(argv[2]);

	//OBTENEMOS LOS DATOS DEL SERVIDOR
 	server.sin_family = AF_INET;
   	server.sin_port = htons(port);
    	server.sin_addr.s_addr = inet_addr(argv[1]);

	//datos del cliente
	client.sin_family = AF_INET;
	client.sin_port = htons(0);
	client.sin_addr.s_addr = INADDR_ANY;

	//ABRO DEL SOCKET
	if((sd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		printf("Error de apertura del socket.\n");
		exit(-1);
	}

	//NOS CONECTAMOS AL SERVIDOR
	if(connect(sd,(struct sockaddr*)&server,sizeof(server)) == -1){
		printf("Error: no se pudo conectar con el servidor.\n");
		exit(-1);
	}
	
	//luego de conectarnos queremos saber nuestro propio puerto
	//para eso necesitamos un socket para el cliente (previamente definido)
	int cl_sz = sizeof(client);
	getsockname(sd,(struct sockaddr*)&client,&cl_sz);
	printf("My port is %d.\n",ntohs(client.sin_port));
	
    	//LEEMOS EL MENSAJE EN EL FICHERO Y LO COPIAMOS EN EL BUFFER
    	if(read(sd,buf,sizeof(buf)) == -1){
        	printf("Error: no se pudo leer el mensaje del servidor.\n");
        	exit(-1);
   	}

    	printf("Mensaje del servidor: %s\n",buf);
	
	//EL USUARIO DEBE LOGUEARSE
	authenticate(sd);
	
	//EL USUARIO PUEDE REALIZAR OPERACIONES
	operate(sd);

	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE
	close(sd);

	return 0;


}







