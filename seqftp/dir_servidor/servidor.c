#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MSG_220 "220 srvftp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "229 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"
#define MSG_200 "200 PORT command successful\r\n"

#define BYTES 512
#define SIZE 100
#define CMD_SIZE 4 	//tamaño de los comandos para las operaciones

bool port(char[],int,struct sockaddr_in);


bool check_credentials(char *user, char *pass){

	FILE *file;
	char *line = NULL;
	size_t len = 0;
	char buf[SIZE];
	
	char *temp;
	char delimitador[] = "\n";

	//CREAMOS EL STRING CON EL USUARIO Y CONTRASEÑA DADO POR EL CLIENTE
	strcat(strcpy(buf,user),":");
	strcat(buf,pass);

	//CHEQUEAMOS QUE EXISTA EL ARCHIVO 'ftpusers.txt'
	file = fopen("ftpusers.txt","r");
	if(file == NULL){
		perror("Error: ");	//errno("Errno: %d",errno);
		return false;
	}
	
	//BUSCAMOS EL STRING CORRESPONDIENTE
	while(getline(&line,&len,file) != -1){
		
		temp = strtok(line, delimitador);

		if(strcmp(buf,temp) == 0){
			
			memset(buf,0,sizeof(buf));
        		fclose(file);
			temp = NULL;
        		free(line);     //o ponerlo en NULL
			
			return true;
		}

	}

	//CERRAMOS ARCHIVO Y LIMPIAMOS VARIABLES
	memset(buf,0,sizeof(buf));
	fclose(file);
	free(line);	//o ponerlo en NULL

	return false;

}

bool authenticate(int sd2){
	
	char buf[SIZE];

	char user[SIZE], pass[SIZE];
        char pass_req[SIZE];    //MSG_331
        char *token = NULL;
	
	while(true){

	//CAPTURAMOS EL NOMBRE DE USUARIO DEL CLIENTE
        if(read(sd2,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del cliente.\n");
                exit(-1);
        }

        buf[strcspn(buf, "\r\n")] = 0;

        token = strtok(buf, " ");

        if(strcmp(token,"USER") == 0 && strlen(token) != 4){
                printf("%s no es un comando FTP valido.\n",token);
                return false;
        }

	token = strtok(NULL," ");
        strcpy(user,token);

        sprintf(pass_req,MSG_331,token);

        memset(buf,0,sizeof(buf));
        strcpy(buf,pass_req);

        if(write(sd2,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo enviar el mensaje requiriendo la contraseña.\n");
                exit(-1);
        }

        memset(buf,0,sizeof(buf));
	token = NULL;			//chequear que este limpio

	//CAPTURAMOS LA CONTRASEÑA DEL CLIENTE
	if(read(sd2,buf,sizeof(buf)) == -1){
                printf("Error: no se pudo leer el mensaje del cliente (2).\n");
                exit(-1);
        }

        buf[strcspn(buf, "\r\n")] = 0;

        token = strtok(buf, " ");

        if(strcmp(token,"PASS") == 0 && strlen(token) != 4){
                printf("%s no es un comando FTP valido.\n",token);
                return false;
        }

        token = strtok(NULL," ");
        strcpy(pass,token);

        memset(buf,0,sizeof(buf));
	token = NULL;

	//PROCEDEMOS A AUTENTICAR AL CLIENTE
	
	if(check_credentials(user,pass)==true){
		
		memset(pass_req,0,sizeof(pass_req));    //este arreglo se reutiliza de forma temporal para evitar crear otro arreglo
                sprintf(pass_req,MSG_230,user);
                strcpy(buf,pass_req);

                if(write(sd2,buf,sizeof(buf)) == -1){
                        printf("Error: no se pudo enviar el mensaje.\n");
                        exit(-1);
                }
		
		memset(buf,0,sizeof(buf));

		return true;	
	}
	else{
		
		strcpy(buf,MSG_530);

                if(write(sd2,buf,sizeof(buf)) == -1){
                        printf("Error: no se pudo enviar el mensaje.\n");
                        exit(-1);
                }
		
		memset(buf,0,sizeof(buf));
		
		//return false;
	}

	}
	return false;
}


void retr(int sd2, char *file_path, int c_port, bool act_mode, int fh_port, struct sockaddr_in port_addr){

	FILE *file;
	long size;
	char buf[BYTES];
	struct stat sb;
	
	int fh; //se usa para la conexion con el cliente
	struct sockaddr_in server;

	if(act_mode == false){

		//obtenemos los datos del cliente
		c_port+=1;
		server.sin_family = AF_INET;
		server.sin_port = htons(c_port);
		server.sin_addr.s_addr = inet_addr("127.0.0.1");

		//abro el socket
		if((fh = socket(AF_INET,SOCK_STREAM,0)) < 0){
			printf("Error de apertura del socket.\n");
			exit(-1);
		}

	}
	
	//CHEQUEAR QUE EL ARCHIVO EXISTA, SINO INFORMAR DEL ERROR AL CLIENTE
	file = fopen(file_path,"r");

	if(file == NULL){
        	strcpy(buf,MSG_550);

                if(write(sd2,buf,sizeof(buf)) == -1){
                        printf("Error: no se pudo enviar el mensaje.\n");
                        exit(-1);
                }
		memset(buf,0,sizeof(buf));
		exit(-1);
	}
	else{
		//SI PUDIMOS ABRIR EL FICHERO, LE ENVIAMOS AL CLIENTE EL TAMAÑO DEL ARCHIVO
	
		if(stat(file_path, &sb) == -1){
			perror("stat");
			exit(-1);
		}
		
		size = sb.st_size;

		sprintf(buf,MSG_299,file_path,size);

		if(write(sd2,buf,sizeof(buf)) == -1){
                        printf("Error: no se pudo enviar el mensaje.\n");
                        exit(-1);
                }

                memset(buf,0,sizeof(buf));

		//DELAY PARA EVITAR PROBLEMAS CON EL BUFFER
		sleep(1);
		
		//establecemos conexion con el cliente
		
		//NOTA: hacer funcion para ambas ramas, ya que hacen lo mismo
		if(act_mode == false){

			if(connect(fh,(struct sockaddr*)&server,sizeof(server)) < 0){
				printf("Error: no se pudo conectar con el cliente.\n");
				exit(-1);
			}

		//cambiamos el file handler original por fh

		//ENVIAMOS EL ARCHIVO
		
		while(!feof(file)){
			fread(buf,sizeof(char),BYTES,file);
			if(write(fh,buf,sizeof(buf)) == -1){
	                        printf("Error: no se pudo transferir el archivo.\n");
                        	exit(-1);
                	}
			memset(buf,0,sizeof(buf));
		}

		memset(buf,0,sizeof(buf));
		}

		else{
		
			if(connect(fh_port,(struct sockaddr*)&port_addr,sizeof(port_addr)) < 0){
                                printf("Error: no se pudo conectar con el cliente.\n");
                                exit(-1);
                        }

			//enviar mensaje de confirmacion (por canal de comandos)
			//el cliente debe leer el mensaje para comprobarlo
			
			strcpy(buf,MSG_200);
			if(write(sd2,buf,sizeof(buf)) == -1){
                                printf("Error: no se pudo transferir el archivo.\n");
                                exit(-1);
                        }
                        memset(buf,0,sizeof(buf));

			//enviar el archivo
			while(!feof(file)){
                        	fread(buf,sizeof(char),BYTES,file);
                        	if(write(fh_port,buf,sizeof(buf)) == -1){
                                	printf("Error: no se pudo transferir el archivo.\n");
                        	        exit(-1);
                        	}
                        	memset(buf,0,sizeof(buf));
                	}

                	memset(buf,0,sizeof(buf));
		}

		sleep(1);

		//ENVIAMOS UN MENSAJE DE QUE SE COMPLETO LA TRANSFERENCIA
		
		strcpy(buf,MSG_226);

		if(write(fh,buf,sizeof(buf)) == -1){
               		printf("Error: no se pudo enviar la confirmacion.\n");
                        exit(-1);
                }
		
		memset(buf,0,sizeof(buf));
		fclose(file);
		close(fh);
		close(fh_port);
	}
}


void operate(int sd2, int c_port){

	char oper[CMD_SIZE], param[SIZE];
	char buf[SIZE];
	char *token = NULL;
	
	//para el comando PORT
	struct sockaddr_in port_addr;
        int fh = 0;
	bool act_mode = false;

	while(true){

		//CHEQUEAMOS LOS COMANDOS ENVIADOS POR EL CLIENTE
		if(read(sd2,buf,sizeof(buf)) == -1){
                	printf("Error: no se pudo leer el mensaje del cliente.\n");
                	exit(-1);
        	}

		buf[strcspn(buf, "\r\n")] = 0;
		token = strtok(buf, " ");
		strcpy(oper,token);

		//ELEGIMOS LA BIFURCACION EN BASE AL COMANDO RECIBIDO
		
			//agregamos el comando port
		if(strncmp(oper, "PORT",4) == 0){
			act_mode = port(buf,fh,port_addr);
		}
		else if(strcmp(oper, "RETR") == 0){
			
			token = strtok(NULL," ");
        		strcpy(param,token);
			token = NULL;
			retr(sd2,param,c_port,act_mode,fh,port_addr);

		}
		else if(strcmp(oper, "QUIT") == 0){
			
			//ENVIO AL CLIENTE EL MSG GOODBYE Y CIERRO LA CONEXION
			
			memset(buf,0,sizeof(buf));

			strcpy(buf,MSG_221);

			if(write(sd2,buf,sizeof(buf)) == -1){
                        	printf("Error: no se pudo enviar la confirmacion.\n");
                        	exit(-1);
                	}

			break;

		}
		else{
			printf("TODO: invalid command.\n");
		}

		memset(buf,0,sizeof(buf));

	}
	memset(buf,0,sizeof(buf));

}


int main(int argc, char *argv[]){

	int sd;	 // descriptor del socket del lado del servidor
	int sd2; // descriptor para hablar con el cliente, canal de comandos
	int port;
	unsigned int length;
    	char buf[SIZE];

	struct sockaddr_in addr;

	//VERIFICAMOS QUE SE HAYA AGREGADO UN SOLO ARGUMENTO
	if(argc != 2){
		printf("Cantidad de argumentos invalida.\n");
		exit(-1);
	}

	port = atoi(argv[1]);

	//ASIGNAMOS PUERTO E IP AL SOCKET
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
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
	if(listen(sd,2) == -1){
		printf("No se ha podido levantar la atencion.\n");
		exit(-1);
	}

	while(true){
		
		//RECIBIMOS A LOS CLIENTES QUE ENTREN EN LA LLAMADA
		length = sizeof(addr);
		sd2 = accept(sd,(struct sockaddr*)&addr,(socklen_t*)&length);
		if(sd2 == -1){
			printf("Error de conexion.\n");
			exit(-1);
		}
		
		//SE MUESTRAN LAS IP Y PUERTO DE AMBOS	
		char ip[INET_ADDRSTRLEN];
		char ip_client[INET_ADDRSTRLEN];
		strcpy(ip_client, inet_ntoa(addr.sin_addr));
		
		inet_ntop(AF_INET,&(addr.sin_addr),ip,INET_ADDRSTRLEN);
		printf("Conexion establecida con IP (servidor): %s / PORT (servidor): %d e IP (cliente): %s / PORT (cliente): %d\n",ip,port,ip_client,ntohs(addr.sin_port));
		int c_port = ntohs(addr.sin_port);

		//ENVIAMOS UN MENSAJE DE SALUDO AL CLIENTE
		strcpy(buf,MSG_220);

	    	if(write(sd2,buf,sizeof(buf)) == -1){
        		printf("Error: no se pudo enviar el mensaje.\n");
	        	exit(-1);
	    	}
	
		memset(buf,0,sizeof(buf));

		//CORROBORAMOS QUE EL USUARIO SE LOGUEO CORRECTAMENTE
		if(authenticate(sd2)==true){
		
			//RECIBIMOS LOS COMANDOS DEL CLIENTE
			operate(sd2, c_port);	
			
		}
		close(sd2);
	}
	
	
	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE Y LUEGO DEL SERVIDOR
    	close(sd2);
    	close(sd);

	return 0;

}


bool port(char buf[],int fh, struct sockaddr_in port_addr){
	
	//variables usadas para obtener los datos del PORT
        unsigned char port[2];
	int port_dec;

	//variables para asociar el socket
        int ip[4];
        char ip_decimal[40];
        
	//struct sockaddr_in port_addr;
	//int fh;

	fh = socket(AF_INET, SOCK_STREAM, 0);

	sscanf(buf,"PORT %d,%d,%d,%d,%d,%d",&ip[0],&ip[1],&ip[2],&ip[3],(int*)&port[0],(int*)&port[1]);
        port_addr.sin_family=AF_INET;
        sprintf(ip_decimal, "%d.%d.%d.%d", ip[0], ip[1], ip[2],ip[3]);
        printf("IP is %s\n",ip_decimal);
        port_addr.sin_addr.s_addr = inet_addr(ip_decimal);
        port_dec = port[0]*256+port[1];
        printf("Port is %d\n",port_dec);
	port_addr.sin_port = htons(port_dec);	

	//luego vendria el connect, pero eso se hace antes de transferir
	return true;
}
