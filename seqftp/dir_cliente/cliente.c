#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <termios.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>

#include <fcntl.h>

#define BUFF_SIZE 100
#define BYTES 512

#define PORT "PORT %d,%d,%d,%d,%d,%d\r\n"

int validar_nro(char*);
int validar_ip(char*);
void send_msg(int,char*,char*);
char* read_input();
void authenticate(int);
void get(int,char*,int);
void quit(int);
void cd(int);
void dir(int);
void addDir(int,char *);
void delDir(int,char *);
bool checkIfFileExists(char *,int);
void operate(int,int);

//ruta dinamica para moverme en el servidor
char dpath[BYTES] = "/home/styvien/Documentos/Redes-II-2022/seqftp/dir_servidor/";

int main(int argc, char *argv[]){

	int sd;
	int port;
	char buf[BUFF_SIZE] = {'\0'};
	struct sockaddr_in server;
	struct sockaddr_in client;

	//VERIFICAMOS QUE SE HAYAN AGREGADO DOS ARGUMENTOS
	if(argc != 3){
		printf("Cantidad de argumentos invalida.\n");
		exit(-1);
	}
	
	char ip[16] = {'\0'};
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
	char *ptr = NULL;
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
	
	char buff[BUFF_SIZE] = {'\0'};

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
		if(strlen(input) <= 1){
			return NULL;
		}else{
			return strtok(input,"\n");
		}
	}
	return NULL;
}

void authenticate(int sd){
	
	char *input = NULL;
	char buf[BUFF_SIZE] = {'\0'};
	char *token = NULL;

	bool confirm_log = false;
	
	struct termios hidden_pass;

	while(confirm_log == false){
	
		//ENVIO EL NOMBRE DEL USUARIO AL SERVIDOR
		printf("Nombre de usuario: ");
		input = read_input();
		while(input == NULL){
			printf("No puede ingresar un usuario vacio. Intente nuevamente: ");
			input = read_input();
		}
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
		
		tcgetattr(fileno(stdin),&hidden_pass);
		hidden_pass.c_lflag &= ~ECHO;
		tcsetattr(fileno(stdin),0,&hidden_pass);
		input = read_input();	//con termios oculto la password
		hidden_pass.c_lflag |= ECHO;
		tcsetattr(fileno(stdin),0,&hidden_pass);

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

	FILE *file = NULL;
	char buf[BYTES] = {'\0'};

	//para crear el canal de datos el cliente toma el rol del servidor
	
	int fh_c, fh_s;			//file handler del cliente y servidor
	struct sockaddr_in client;
	int p1, p2;			//para los args del PORT
	int readed = 0;			//lo leido por el cliente cuando escribe en el archivo

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
		
		if(read(sd,buf,sizeof(buf)) < 0){
			printf("Error: no se pudo leer el mensaje del servidor.\n");
                	exit(-1);
        	}
		printf("Mensaje del servidor: %s\n\n",buf);
		memset(buf,0,sizeof(buf));

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
	
	//cambiamos el file handler original por fh_s
	
	//RECIBIMOS EL ARCHIVO
	file = fopen(file_name, "w");
	
	if(file == NULL){
		printf("Error: no se pudo abrir el archivo en modo escritura.\n");
		exit(-1);
	}

	//VOLCAMOS EL CONTENIDO EN EL ARCHIVO ABIERTO QUE SE ENCUENTRA EN EL DIRECTORIO DEL CLIENTE
	
	do{
		if((readed = read(fh_s,buf,sizeof(buf))) < 0){
	         	printf("Error: no se pudo leer el mensaje del servidor.\n");
        		exit(-1);
        	}
        	fwrite(buf,1,readed,file);
	}while(readed == BYTES);

	//RECIBIMOS EL MENSAJE DE TRANSFERENCIA COMPLETA POR PARTE DEL SERVIDOR
	if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

	printf("\nMensaje del servidor: %s\n\n",buf);

	memset(buf,0,sizeof(buf));
	
	close(fh_c); close(fh_s);	//cerramos los file handler usados
	fclose(file);
	
}

void quit(int sd){
	
	char buf[BUFF_SIZE] = {'\0'};

	send_msg(sd,"QUIT",NULL);

	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }
	
	printf("Mensaje del servidor: %s\n",buf);

	memset(buf,0,sizeof(buf));
}

bool checkIfFileExists(char *param,int bit){
	struct stat buffer;
	
	char temp[BYTES] = {'\0'};

	if(strcmp(param,"..") == 0){
		memset(dpath,0,sizeof(0));
		strcpy(dpath,"/home/styvien/Documentos/Redes-II-2022/seqftp/dir_servidor");
		return true;
	}
	else{

		strcpy(temp,dpath);

	    	strcat(dpath,param);
		
		int exist = stat(dpath,&buffer);
	   	
		if(bit == 0){
			memset(dpath,0,sizeof(dpath));
			strcpy(dpath,temp);
		}
		
		printf("\n%s\n",dpath);

		if(exist == 0){
	        	return true;
		}else{  
	        	memset(dpath,0,sizeof(dpath));
			strcpy(dpath,temp);
			return false;
		}
		
		memset(temp,0,sizeof(temp));

	}

}

void help(){

	printf("\n -> get: pide un archivo existente en el servidor, y el servidor lo retorna al cliente.\n -> mkdir: crea un directorio del lado del cliente.\n -> rmdir: elimina un directorio del lado del cliente.\n -> dir: lista los archivos de un directorio del lado del servidor.\n -> cd: permite movernos por directorios del lado del servidor.\n -> quit: cortamos la comunicacion con el servidor.\n\n");

}

//CREO QUE DEBO RETORNAR LA RUTA DE ALGUNA MANERA (PARA EL DIR U OTROS MOVIMIENTOS)
void cd(int sd){
	
	char buf[BYTES] = {'\0'};

	send_msg(sd,"CWD",dpath);

	if(read(sd,buf,sizeof(buf)) < 0){
		printf("Error: no pudo leerse la respuesta del servidor (cd).\n");
	}

	printf("\nMensaje del servidor: %s\n",buf);
	memset(buf,0,sizeof(buf));
}


void dir(int sd){
	
	char buf[BYTES] = {'\0'};

	printf("\n%s\n",dpath);
        send_msg(sd,"NLIST",dpath);
	
	if(read(sd,buf,sizeof(buf)) < 0){
        	printf("Error: no se puede leer la lista de archivos.\n");
                exit(-1);
        }
	
	printf("\n%s\n",buf);
	memset(buf,0,sizeof(buf));
}

//TENGO QUE CHEQUEAR POR LAS DUDAS DONDE QUIERE CREAR LA CARPETA, Y PASARLE CORRECTAMENTE EL PATH

void addDir(int sd, char *param){
	char buf[BUFF_SIZE] = {'\0'};
	
	//para crear el directorio, necesito el path del cliente
	//dpath = PWD_CLIENTE;

        send_msg(sd,"MKD",param);
	
	if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se puede recibir la confirmacion del servidor.\n");
                exit(-1);
        }
        printf("\n%s\n",buf);

        memset(buf,0,sizeof(buf));	
}


void delDir(int sd, char *param){
	
	char buf[BUFF_SIZE] = {'\0'};
	
	//dpath = PWD_CLIENTE;

        send_msg(sd,"RMD",param);
	
        if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se puede recibir la confirmacion del servidor.\n");
                exit(-1);
        }
        printf("\n%s\n",buf);
        memset(buf,0,sizeof(buf));
}


void operate(int sd, int c_port){

	char *input = NULL; char *oper = NULL; char *param = NULL;

	while(true){
		printf("\nSi no sabe que significa cada operacion, escriba 'help'.\n");
		printf("\nIngrese una operacion ('get file_name.txt' | 'mkdir directory_name' | 'rmdir directory_name' | 'dir' | 'cd directory_name' | 'quit'): ");

		input = read_input();

		if(input == NULL){
			continue;
		}

		oper = strtok(input, " ");

		if(strcmp(oper,"get") == 0){
			param = strtok(NULL, " ");
			
			if(checkIfFileExists(param,0) == true){
				get(sd,param,c_port);
    			}
			else{
				printf("Ingrese correctamente el nombre del archivo.\n");
				oper = NULL; param = NULL;	//creo que estoy dejando basura en el sistema
				continue;
			}
		
		}
		else if(strcmp(oper,"dir") == 0){
			dir(sd);
			
		}
		else if(strcmp(oper,"cd") == 0){
			
			param = strtok(NULL, " ");
			
			if(checkIfFileExists(param,1) == true){
				strcat(dpath,"/");
				cd(sd);
			}
			else{
				printf("\nIngrese correctamente el nombre del directorio.\n\n");
			}

		}
		else if(strcmp(oper,"mkdir") == 0){
			
			param = strtok(NULL, " ");
			
			printf("param: %s\n",param);
			
                        addDir(sd,param);
                        
		}
		else if(strcmp(oper,"rmdir") == 0){
			
			param = strtok(NULL, " ");
			
                        printf("param: %s\n",param);
			
                        delDir(sd,param);

		}
		else if(strcmp(oper,"help") == 0){
			help();

		}
		else if(strcmp(oper,"quit") == 0){
			
			quit(sd);

			break;
		}
		else{
			printf("Error: comando desconocido.\n\n");
		}
		free(input);
		oper = NULL; param = NULL;
	}
	free(input);

}

