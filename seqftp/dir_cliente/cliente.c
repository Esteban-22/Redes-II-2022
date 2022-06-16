#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <termios.h>

//TAMAÑOS DEFINIDOS PARA ARREGLOS
#define SIZE 100
#define BYTES 512

//COMANDO QUE LE ENVIA EL CLIENTE AL SERVIDOR
#define PORT "PORT %d,%d,%d,%d,%d,%d\r\n"

//#ifdef
#define DIRECTIVE

//PROTOTIPOS
int validar_nro(char*);
int validar_ip(char*);
void send_msg(int,char*,char*);
char* read_input();
void authenticate(int);
void get(int,char*,int);
void quit(int);
void cd(int);
void dir(int);
void addDir(int);
void delDir(int);
bool checkIfFileExists(char *,int);
void operate(int,int);

//RUTA DINAMICA (QUE CAMBIA) PARA MOVERME EN EL SERVIDOR
char dpath[BYTES] = "/home/styvien/Documentos/REDES-II-2022/seqftp/dir_servidor/";


int main(int argc, char *argv[]){

	int sd;
	int port;
	char buf[SIZE] = {'\0'};
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

	//VALIDAMOS EL FORMATO DE LA DIRECCION IP (v4)
    	validar_ip(ip);

	//GUARDAMOS EL PUERTO
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
	//printf("My port is %d.\n",ntohs(client.sin_port));
	//printf("My IP address is %s.\n",inet_ntoa(client.sin_addr));
	
	int c_port = ntohs(client.sin_port);

    	//LEEMOS EL MENSAJE Y LO COPIAMOS EN EL BUFFER
    	if(read(sd,buf,sizeof(buf)) == -1){
        	printf("Error: no se pudo leer el mensaje del servidor.\n");
        	exit(-1);
   	}

   	#ifdef DIRECTIVE
	#else
    	printf("Mensaje del servidor: %s\n",buf);
	#endif

	//EL USUARIO SE LOGUEA
	authenticate(sd);
	
	//EL USUARIO REALIZA OPERACIONES
	operate(sd,c_port);
	
	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE
	close(sd);

	return 0;


}

//FUNCION QUE SE LLAMA EN validar_ip()
int validar_nro(char *str){
	while(*str){
		if(!isdigit(*str)){ //si el char no es un nro, retorna 'false'
			return 0;
		}
		str++;
	}
	return 1;
}

//FUNCION QUE CHEQUEA SI LA IP TIENE UN FORMATO VALIDO
int validar_ip(char *ip){
	int num, dot = 0;
	char *ptr = NULL;
	if(ip == NULL){
		return 0;
	}
	ptr = strtok(ip,"."); 			//corta el string si encuentra un punto
	if(ptr == NULL){
		return 0;
	}
	while(ptr){
		if(!validar_nro(ptr)){		//chequea si los nros entre puntos son validos
			return 0;
		}
		num = atoi(ptr);		//convierte el string a un nro
		if(num >= 0 && num <= 255){
			ptr = strtok(NULL,".");
			if(ptr != NULL){
				dot++;		// incrementa el contador de puntos
			}
		} else {
			return 0;
		}
	}
	if(dot != 3){
		return 0;
	}
	return 1;
}

//ENVIO COMANDOS AL SERVIDOR
void send_msg(int sd, char *oper, char *param){
	
	char buf[SIZE] = {'\0'};

	if(param != NULL){
		sprintf(buf, "%s %s\r\n",oper,param);
	}
	else{
		sprintf(buf, "%s\r\n",oper);
	}
	
	if(write(sd,buf,sizeof(buf)) == -1){
		printf("Error: no se pudo enviar el mensaje dentro de la funcion send_msg().\n");
                exit(-1);
	}

	memset(buf,0,sizeof(buf));
}

//LEO LO QUE EL CLIENTE INGRESE POR TECLADO
char* read_input(){

	char *input = malloc(SIZE);
	if(fgets(input,SIZE,stdin)){
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
	char buf[SIZE] = {'\0'};
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

        	#ifdef DIRECTIVE
		#else
	        printf("Mensaje del servidor: %s\n",buf);
		#endif

		memset(buf,0,sizeof(buf));

		//ENVIO LA CONTRASEÑA AL SERVIDOR
		printf("Contraseña: ");
		
		//OCULTO LA CONTRASEÑA
		tcgetattr(fileno(stdin),&hidden_pass);
		hidden_pass.c_lflag &= ~ECHO;
		tcsetattr(fileno(stdin),0,&hidden_pass);
		input = read_input();
		hidden_pass.c_lflag |= ECHO;
		tcsetattr(fileno(stdin),0,&hidden_pass);

		printf("\n");
		send_msg(sd,"PASS",input);

		free(input);

		//LEEMOS LA RESPUESTA DEL SERVIDOR TRAS ANALIZAR NUESTRO USUARIO Y CONTRASEÑA
		if(read(sd,buf,sizeof(buf)) == -1){
        	        printf("Error: no se pudo leer el mensaje del servidor.\n");
                	exit(-1);
	        }

	        #ifdef DIRECTIVE
		#else
        	printf("Mensaje del servidor: %s\n",buf);
		#endif

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

	//PARA EL CANAL DE DATOS, EL CLIENTE TOMA EL ROL DE SERVIDOR
	
	int fh_c, fh_s;			//file handler del cliente y del servidor
	struct sockaddr_in client;
	int p1, p2;			//para los args del PORT
	int readed = 0;			//lo leido por el cliente cuando escribe en el archivo

	//ASIGNAMOS PUERTO E IP AL SOCKET
	c_port+=1;
	client.sin_family = AF_INET;
	client.sin_port = htons(c_port);
	client.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	//ABRO EL SOCKET
	if((fh_c = socket(AF_INET,SOCK_STREAM,0)) < 0){
		printf("Error de apertura del socket.\n");
		exit(-1);
	}
	
	//ASOCIAMOS PUERTO E IP AL SOCKET
	if(bind(fh_c,(struct sockaddr*)&client,sizeof(client)) < 0){
		
		printf("Error de asociacion de socket (error en bind()).\n");
		
		//SI HAY ERROR DE ASOCIACION, ASIGNO CON PORT
		client.sin_port = htons(0);
		if(bind(fh_c, (struct sockaddr*)&client,sizeof(client)) <0){
			printf("Error al asignar un puerto al socket.\n");
			exit(-1);
		}
		
		int cl_sz = sizeof(client);
        	getsockname(fh_c,(struct sockaddr*)&client,(socklen_t*)&cl_sz);
	        c_port = ntohs(client.sin_port);
		//printf("My port is %d.\n",c_port);
		
		//OBTENEMOS LOS PUERTOS PARA LOS ARGUMENTOS DE PORT
		p1 = c_port % 256;
		p2 = c_port - (p1*256);

		//ENVIAMOS EL NUEVO PUERTO AL SERVIDOR
		
		sprintf(buf,PORT,127,0,0,1,p1,p2);
		
		if(write(sd,buf,sizeof(buf)) == -1){		//uso el file handler sd, porque envio un comando
                        printf("Error: no se pudo enviar el mensaje.\n");
                        exit(-1);
                }

                memset(buf,0,sizeof(buf));
		
		if(read(sd,buf,sizeof(buf)) < 0){
			printf("Error: no se pudo leer el mensaje del servidor (port).\n");
                	exit(-1);
        	}

        	#ifdef DIRECTIVE
		#else
		printf("Mensaje del servidor: %s\n\n",buf);
		#endif

		memset(buf,0,sizeof(buf));

	}
	
	//ESCUCHAMOS
	if(listen(fh_c,1) < 0){
		printf("No se ha podido levantar la atencion.\n");
		exit(-1);
	}

	//ENVIAMOS EL COMANDO 'RETR' AL SERVIDOR
	send_msg(sd,"RETR",file_name);		//file_name es el parametro (el nombre del archivo)

	//RECIBIMOS LA RESPUESTA DEL SERVIDOR QUE CONTIENE EL TAMAÑO DEL ARCHIVO
	if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se pudo leer el mensaje del servidor (size).\n");
                perror("Error: ");
                exit(-1);
        }

        #ifdef DIRECTIVE
	#else
        printf("Mensaje del servidor: %s\n",buf);
        #endif

        memset(buf,0,sizeof(buf));

	//ACEPTAMOS LA PETICION DE CONEXION DEL SERVIDOR
	unsigned int len = sizeof(client);
	if((fh_s = accept(fh_c,(struct sockaddr*)&client,(socklen_t*)&len)) < 0){
		printf("Error: no se pudo recibir al servidor.\n");
		exit(-1);
	}
	
	//RECIBIMOS EL ARCHIVO
	file = fopen(file_name, "w");
	
	if(file == NULL){
		printf("Error: no se pudo abrir el archivo en modo escritura.\n");
		exit(-1);
	}

	//VOLCAMOS EL CONTENIDO EN EL ARCHIVO ABIERTO QUE SE ENCUENTRA EN EL DIRECTORIO DEL CLIENTE
	
	do{
		if((readed = read(fh_s,buf,sizeof(buf))) < 0){
	         	printf("Error: no se pudo leer el mensaje del servidor (readed).\n");
        		exit(-1);
        	}
        	fwrite(buf,1,readed,file);

	}while(readed == BYTES);

	//RECIBIMOS EL MENSAJE DE TRANSFERENCIA COMPLETA POR PARTE DEL SERVIDOR
	if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se pudo leer el mensaje del servidor (transfer).\n");
                exit(-1);
        }

        #ifdef DIRECTIVE
	#else
	printf("\nMensaje del servidor: %s\n\n",buf);
	#endif

	memset(buf,0,sizeof(buf));
	
	close(fh_c); close(fh_s);	//cerramos los file handler usados
	fclose(file);
	
}

void quit(int sd){
	
	char buf[SIZE] = {'\0'};

	send_msg(sd,"QUIT",NULL);

	if(read(sd,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del servidor.\n");
                exit(-1);
        }

	#ifdef DIRECTIVE
	#else
	printf("Mensaje del servidor: %s\n",buf);
	#endif

	memset(buf,0,sizeof(buf));
}

//SIRVE PARA CHEQUEAR LA EXISTENCIA DE ARCHIVOS O CARPETAS
bool checkIfFileExists(char *param,int bit){
	
	struct stat buffer;
	
	char temp[BYTES] = {'\0'};

	if(strcmp(param,"..") == 0){
		memset(dpath,0,sizeof(0));
		strcpy(dpath,"/home/styvien/Documentos/REDES-II-2022/seqftp/dir_servidor");
		return true;
	}
	else{

		strcpy(temp,dpath);

	    	strcat(dpath,param);

		int exist = stat(dpath,&buffer);
	   	
	   	//bit sirve para saber si estoy comparando un archivo o un directorio
		if(bit == 0){
			memset(dpath,0,sizeof(dpath));
			strcpy(dpath,temp);
		}

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

//AYUDAS AL USUARIO
void help(){

	printf(	"\nDIRECTIVAS:\n"
		"   -> get: pide un archivo existente en el servidor, y el servidor lo retorna al cliente.\n"
		"   -> mkdir: crea un directorio del lado del cliente.\n"
		"   -> rmdir: elimina un directorio del lado del cliente.\n"
		"   -> dir: lista los archivos de un directorio del lado del servidor.\n"
		"   -> cd: permite movernos por directorios del lado del servidor.\n"
		"   -> quit: cortamos la comunicacion con el servidor.\n\n");

}

void cd(int sd){
	
	char buf[BYTES] = {'\0'};

	send_msg(sd,"CWD",dpath);

	if(read(sd,buf,sizeof(buf)) < 0){
		printf("Error: no pudo leerse la respuesta del servidor (cd).\n");
	}

	printf("\n%s\n",dpath);

	#ifdef DIRECTIVE
	#else
	printf("\nMensaje del servidor: %s\n",buf);
	#endif

	memset(buf,0,sizeof(buf));
}


void dir(int sd){
	
	char buf[BYTES] = {'\0'};

        send_msg(sd,"NLIST",dpath);
	
	if(read(sd,buf,sizeof(buf)) < 0){
        	printf("Error: no se puede leer la lista de archivos.\n");
                exit(-1);
        }
	
	printf("\n%s\n",buf);
	memset(buf,0,sizeof(buf));
}


void addDir(int sd){
	
	char buf[BYTES] = {'\0'};

        send_msg(sd,"MKD",dpath);
	
	if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se puede recibir la confirmacion del servidor.\n");
                exit(-1);
        }
        
        printf("\n%s\n",buf);
        memset(buf,0,sizeof(buf));

}


void delDir(int sd){
	
	char buf[BYTES] = {'\0'};
	
        send_msg(sd,"RMD",dpath);
	
        if(read(sd,buf,sizeof(buf)) < 0){
                printf("Error: no se puede recibir la confirmacion del servidor.\n");
                exit(-1);
        }
        
        printf("\n%s\n",buf);
        memset(buf,0,sizeof(buf));
}

//ADMINISTRA LOS COMANDOS QUE INGRESA EL USUARIO
void operate(int sd, int c_port){

	char *input = NULL; char *oper = NULL; char *param = NULL;
	char temp[BYTES] = {'\0'};

	while(true){
		printf("\nSi no sabe que significa cada operacion, escriba 'help'.\n");
		printf("\nIngrese una operacion:\n -> 'get file_name.txt'\n -> 'mkdir directory_name'\n -> 'rmdir directory_name'\n -> 'dir'\n -> 'cd directory_name'\n -> 'quit'\n\n -> ");

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
				oper = NULL; param = NULL;
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
			
			strcpy(temp,dpath);

			param = strtok(NULL, " ");
			
			strcat(dpath,param);
			strcat(dpath,"/");

                        addDir(sd);

                        memset(dpath,0,sizeof(dpath));
                        strcpy(dpath,temp);
                        memset(temp,0,sizeof(temp));
                        
		}
		else if(strcmp(oper,"rmdir") == 0){
			
			strcpy(temp,dpath);

			param = strtok(NULL, " ");
			
			strcat(dpath,param);
			strcat(dpath,"/");
                        delDir(sd);

                        memset(dpath,0,sizeof(dpath));
                        strcpy(dpath,temp);
                        memset(temp,0,sizeof(temp));

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

