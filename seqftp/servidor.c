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

#define MSG_220 "220 srvftp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "229 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"

#define SIZE 100

bool check_credentials(char *user, char *pass){

	FILE *file;
	char *line = NULL;
	size_t len = 0;

	bool found = false;		//si se encontro el string
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
        char delimitador[] = "\r\n";
	
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

		return false;
	}


}

int main(int argc, char *argv[]){

	int sd;	 // descriptor del socket
	int sd2; // descriptor para hablar con el cliente
	int puerto;
	unsigned int length;
    	char buf[SIZE];

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
    	
	strcpy(buf,MSG_220);

	//escribimos un mensaje en el fichero del cliente
    	if(write(sd2,buf,sizeof(buf)) == -1){
        	printf("Error: no se pudo enviar el mensaje.\n");
	        exit(-1);
    	}
	
	memset(buf,0,sizeof(buf));

	//CORROBORAMOS QUE EL USUARIO SE LOGUEO CORRECTAMENTE
	if(authenticate(sd2)==true){
		

	}
	else{


	}
	
	//SE CIERRA EL FICHERO DEL LADO DEL CLIENTE Y LUEGO DEL SERVIDOR
    	//close(sd2);
    	//close(sd);

	return 0;

}
