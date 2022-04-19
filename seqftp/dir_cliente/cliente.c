#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <fcntl.h>

#define BUFF_SIZE 100
#define BYTES 512

#define PORT "PORT %d,%d,%d,%d,%d,%d\r\n"

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


void get(int sd, char *file_name, int c_port){

	FILE *file;
	char buf[BYTES];
	char *temp = NULL;
	bool act_mode = false;

	//para crear el canal de datos el cliente toma el rol del servidor
	
	int fh_c, fh_s;			//file handler del cliente y servidor
	struct sockaddr_in client;
	int p1, p2;			//para los args del PORT

	//asignamos puerto e ip al socket
	c_port+=1;
	client.sin_family = AF_INET;
	client.sin_port = htons(c_port);
	client.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	//abro el socket
	if((fh_c = socket(AF_INET,SOCK_STREAM,0)) < 0){
		printf("Error de apertura del socket.\n");
		exit(-1);
	}
	
	//asociamos un puerto e ip al socket
	if(bind(fh_c,(struct sockaddr*)&client,sizeof(client)) < 0){
		printf("Error de asociacion de socket (error en bind()).\n");
		
		//asigno un nuevo puerto con PORT
		client.sin_port = htons(0);
		if(bind(fh_c, (struct sockaddr*)&client,sizeof(client)) <0){
			printf("Error al asignar un puerto al socket.\n");
			exit(-1);
		}
		
		int cl_sz = sizeof(client);
        	getsockname(fh_c,(struct sockaddr*)&client,(socklen_t*)&cl_sz);
	        c_port = ntohs(client.sin_port);
		printf("My port is %d.\n",c_port);
		
		//obtenemos los puertos para los argumentos del PORT
		p1 = c_port % 256;
		p2 = c_port - (p1*256);

		//enviamos el nuevo puerto al servidor
		
		sprintf(buf,PORT,127,0,0,1,p1,p2);
		
		if(write(sd,buf,sizeof(buf)) == -1){	//sd, porque le envio un comando
                        printf("Error: no se pudo enviar el mensaje.\n");
                        exit(-1);
                }

                memset(buf,0,sizeof(buf));
		
		act_mode = true;

	}

	//escuchamos
	//al no ser bloqueante, puedo implementarla antes del RETR
	if(listen(fh_c,1) < 0){
		printf("No se ha podido levantar la atencion.\n");
		exit(-1);
	}

	//ENVIAMOS EL COMANDO 'RETR' AL SERVIDOR
	send_msg(sd,"RETR",file_name);		//file_name es el parametro (el nombre del archivo)

	//RECIBIMOS LA RESPUESTA DEL SERVIDOR QUE CONTIENE EL TAMAÑO DEL ARCHIVO
	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

        printf("Mensaje del servidor: %s\n",buf);
	
        memset(buf,0,sizeof(buf));

	//aceptamos la peticion de conexion del servidor, una vez comencemos la transferencia del archivo
	unsigned int len = sizeof(client);
	if((fh_s = accept(fh_c,(struct sockaddr*)&client,(socklen_t*)&len)) < 0){
		printf("Error: no se pudo recibir al servidor.\n");
		exit(-1);
	}
	
	if(act_mode == true){
		
		if(read(sd,buf,sizeof(buf)) < 0){
	                printf("Error: no se pudo leer el mensaje del servidor.\n");
                	exit(-1);
        	}
		printf("Mensaje del servidor: %s",buf);
		memset(buf,0,sizeof(buf));

	}

	//cambiamos el file handler original por fh_s
	
	//RECIBIMOS EL ARCHIVO
	file = fopen(file_name, "w");
	
	if(file == NULL){
		printf("Error: no se pudo abrir el archivo en modo escritura.\n");
		exit(-1);
	}

	//VOLCAMOS EL CONTENIDO EN EL ARCHIVO ABIERTO QUE SE ENCUENTRA EN EL DIRECTORIO DEL CLIENTE
	
	if(read(fh_s,buf,sizeof(buf)) < 0){
         	printf("Error: no se pudo leer el mensaje del servidor.\n");
        	exit(-1);
        }


	//para evitar que llene el archivo de 512 bytes (aun si no llega a esa cantidad) usamos un puntero temporal, donde copiamos el contenido del buffer
		
	temp = strtok(buf," ");
        fwrite(temp,1,strlen(temp),file);
		
	memset(buf,0,sizeof(buf));
        temp = NULL;

	//RECIBIMOS EL MENSAJE DE TRANSFERENCIA COMPLETA POR PARTE DEL SERVIDOR
	if(read(fh_s,buf,sizeof(buf)) < 0){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

	printf("\nMensaje del servidor: %s\n\n",buf);

	memset(buf,0,sizeof(buf));
	
	close(fh_c); close(fh_s);	//cerramos los file handler usados
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


void operate(int sd, int c_port){

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
			
			get(sd,param,c_port);
		
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

	//DATOS DEL CLIENTE
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
	
	//QUEREMOS SABER NUESTRO PROPIO PUERTO
	int cl_sz = sizeof(client);
	getsockname(sd,(struct sockaddr*)&client,(socklen_t*)&cl_sz);
	printf("My port is %d.\n",ntohs(client.sin_port));
	printf("My IP address is %s.\n",inet_ntoa(client.sin_addr));
	
	int c_port = ntohs(client.sin_port);

    	//LEEMOS EL MENSAJE EN EL FICHERO Y LO COPIAMOS EN EL BUFFER
    	if(read(sd,buf,sizeof(buf)) == -1){
        	printf("Error: no se pudo leer el mensaje del servidor.\n");
        	exit(-1);
   	}

    	printf("Mensaje del servidor: %s\n",buf);
	
	//EL USUARIO DEBE LOGUEARSE
	authenticate(sd);
	
	//EL USUARIO PUEDE REALIZAR OPERACIONES
	operate(sd,c_port);

	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE
	close(sd);

	return 0;


}







