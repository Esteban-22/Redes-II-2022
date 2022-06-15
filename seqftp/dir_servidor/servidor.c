#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define SIZE 100
#define BYTES 512

#define MSG_220 "220 srvftp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "229 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"
#define MSG_200 "200 PORT command successful\r\n"

#define CMDSIZE 4 	//tamaño de los comandos para las operaciones

bool chech_credentials(char*,char*);
bool authenticate(int);
void retr(int,char*,int,bool,int,struct sockaddr_in);
void cwd(int,char*);
void nlist(int,char*);
void addDir(int,char*);
void delDir(int,char*);
void operate(int,int);
void send_file(int,struct sockaddr_in,int,FILE*);

//ruta dinamica para encontrar archivos dentro de carpetas
char dpath[BYTES] = {'\0'};

int main(int argc, char *argv[]){

	int sd;	 // descriptor del socket del lado del servidor
	int sd2; // descriptor para hablar con el cliente, canal de comandos
	int port;
	unsigned int length;
    	char buf[SIZE]={'\0'};

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
	if(listen(sd,5) == -1){
		printf("No se ha podido levantar la atencion.\n");
		exit(-1);
	}

	while(true){
		
		//RECIBIMOS A LOS CLIENTES QUE ENTREN EN LA LLAMADA
		length = sizeof(addr);
		if((sd2 = accept(sd,(struct sockaddr*)&addr,(socklen_t*)&length)) < 0){
			printf("Error de conexion.\n");
			exit(-1);
		}
		
		
		//SE MUESTRAN LAS IP Y PUERTO DE AMBOS	
		char ip[INET_ADDRSTRLEN] = {'\0'};
		char ip_client[INET_ADDRSTRLEN] = {'\0'};
		strcpy(ip_client, inet_ntoa(addr.sin_addr));
		
		inet_ntop(AF_INET,&(addr.sin_addr),ip,INET_ADDRSTRLEN);
		int c_port = ntohs(addr.sin_port);

		//printf("Conexion establecida con IP (servidor): %s / PORT (servidor): %d e IP (cliente): %s / PORT (cliente): %d\n",ip,port,ip_client,ntohs(addr.sin_port));

		

		//INICIO DE LA CONCURRENCIA CON FORK()

		//printf("** Proceso PID inicial: %d\n\n",getpid());

		pid_t pid;
		pid = fork();	//creo un proceso

		//printf("** Proceso PID ejecutandose: %d / PID base: %d \n\n",pid,getpid());

		if(pid == 0){	//se esta ejecutando el hijo

			//el hijo se va a encargar de manejar las peticiones del cliente

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

			close(sd2);	//una vez que el cliente finaliza, cierro su socket asociado
			exit(0); 	//destruyo el proceso hijo

		}
		else if(pid > 0){	//se esta ejecutando el padre

			//el padre no necesita esperar al hijo, ya que debe seguir atendiendo pedidos de conexion entrantes

			//el hijo es una copia del padre y, y el socket que se usa para comunicarse con el cliente esta siendo manejado por el hijo
			//el padre no lo necesita
			close(sd2);	
			
			//vuelvo al inicio del while para crear un nuevo socket con el cliente que se quiere sumar a la conexion y se repite el proceso
			continue;

		}
		else{	//error
			
			//si hay un error, primero tengo que cerrar el socket creado
			close(sd2);
			perror("fork");
			exit(EXIT_FAILURE);

		}

	}

	return 0;

}

bool check_credentials(char *user, char *pass){

	FILE *file = NULL;
	char *line = NULL;
	size_t len = 0;
	char buf[SIZE] = {'\0'};
	
	char *temp = NULL;
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
	
	char buf[SIZE] = {'\0'};

	char user[SIZE] = {'\0'}, pass[SIZE] = {'\0'};
        char pass_req[SIZE] = {'\0'};    //MSG_331
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
		
	}

	}
	return false;
}

//conexion con el cliente y envio del archivo
void send_file(int fh, struct sockaddr_in sock, int readed, FILE *file){
	
	char buf[BYTES] = {'\0'};

	if(connect(fh,(struct sockaddr*)&sock,sizeof(sock)) < 0){
        	printf("Error: no se pudo conectar con el cliente.\n");
                exit(-1);
        }

        //ENVIAMOS EL ARCHIVO

        while(!feof(file)){
        	readed = fread(buf,sizeof(char),BYTES,file);
                if(write(fh,buf,readed) == -1){
                	printf("Error: no se pudo transferir el archivo.\n");
                        exit(-1);
                }
		memset(buf,0,sizeof(buf));
	}
        memset(buf,0,sizeof(buf));
}

void retr(int sd2, char *file_path, int c_port, bool act_mode, int fh_port, struct sockaddr_in port_addr){

	FILE *file = NULL;
	long size;
	char buf[BYTES]={'\0'};
	struct stat sb;
	
	int fh; //se usa para la conexion con el cliente
	struct sockaddr_in server;
	int readed = 0;

	char dpath_temp[BYTES]={'\0'};

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
	
	//dpath_temp guarda la ruta incluyendo al archivo, mientras que dpath guarda la ruta sin el archivo
	strcat(dpath_temp,dpath);
	strcat(dpath_temp,file_path);

	file = fopen(dpath_temp,"r");

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
	
		if(stat(dpath_temp, &sb) == -1){
			perror("stat");
			exit(-1);
		}
		
		size = sb.st_size;

		sprintf(buf,MSG_299,file_path,size);
	
		if(write(sd2,buf,sizeof(buf)) == -1){
                        printf("Error: no se pudo enviar el mensaje(1).\n");
                        exit(-1);
                }
		
                memset(buf,0,sizeof(buf));
	
		//DELAY PARA EVITAR PROBLEMAS CON EL BUFFER
		sleep(1);
		
		//establecemos conexion con el cliente y enviamos el archivo
		
		if(act_mode == false){
			send_file(fh,server,readed,file);
		}

		else{
			send_file(fh_port,port_addr,readed,file);
		}

		sleep(1);

		//ENVIAMOS UN MENSAJE DE QUE SE COMPLETO LA TRANSFERENCIA
		
		strcpy(buf,MSG_226);

		if(write(sd2,buf,sizeof(buf)) == -1){
               		printf("Error: no se pudo enviar la confirmacion.\n");
                        exit(-1);
                }
		
		memset(buf,0,sizeof(buf));
		memset(dpath_temp,0,sizeof(dpath_temp));
		fclose(file);
		close(fh);
		close(fh_port);
	}

}


void cwd(int sd2, char *param){
	
	char buf[BYTES] = {'\0'};
	
	strcpy(buf,param);
	
	//esto sirve si no creo ni envio archivos en o hacia el servidor
	if(strcmp(dpath,param) > 0){	//si hago cd .. borro el contenido de dpath y lo reemplazo por param
		memset(dpath,0,sizeof(dpath));
		strcpy(dpath,param);
	}
	else{
		strcpy(dpath,param);
	}

	if(write(sd2,buf,sizeof(buf)) < 0){
		printf("Error: no se pudo enviar la direccion del directorio solicitado por el cliente.\n");
		exit(-1);
	}
	memset(buf,0,sizeof(buf));

}


void nlist(int sd2, char *param){
	
	struct dirent **resultados = NULL;
	int nroResultados;
	char buf[BYTES] = {'\0'};
	char temp[BYTES] = {'\0'};

	//obtengo ficheros de la ruta indicada
	nroResultados = scandir(param, &resultados, NULL, NULL);

	//guardo la lista en el bufer
	for (int i=0; i<nroResultados; i++){
        	strcpy(temp,resultados[i]->d_name);
		if(i==0){
			strcpy(buf,temp);
			strcat(buf,"\n");
			memset(temp,0,sizeof(temp));
		}
		else{
			strcat(buf,temp);
			strcat(buf,"\n");
			memset(temp,0,sizeof(temp));
		}
	}
	
	//envio la informacion al cliente
        if(write(sd2,buf,sizeof(buf)) < 0){
		printf("Error: no se puede enviar la lista de archivos.\n");
		exit(-1);
	}

	//libero los variables almacenadas dinamicamente
	for (int i=0; i<nroResultados; i++){
		free(resultados[i]);
		resultados[i] = NULL;
	}
	
	memset(buf,0,sizeof(buf));
	free(resultados);
	resultados = NULL;

}

//TIENE CASI LO MISMO QUE DELDIR, POR LO QUE HAY QUE HACER UNA FUNCION PARA AMBAS
void addDir(int sd2, char *param){
	
	int ret;
	char buf[SIZE] = {'\0'};
	errno = 0;

	//las banderas permiten que el user tenga permiso de lectura, escritura y ejecucion, respectivamente
	ret = mkdir(param,S_IRUSR | S_IWUSR | S_IXUSR);
	
	//le aviso al cliente que se creó la carpeta, o no
	if (ret == -1) {
	        switch (errno) {
        	    case EACCES:
	                strcpy(buf,"the parent directory does not allow write");
	                break;
		    case EEXIST:
	                strcpy(buf,"pathname already exists");
	                break;
        	    case ENAMETOOLONG:
	                strcpy(buf,"pathname is too long");
	                break;
        	    default:
			perror("mkdir");
                	exit(EXIT_FAILURE);
        	}
    	}
	else{
		sprintf(buf,"Directory created");
	}
	
	if(write(sd2,buf,sizeof(buf)) < 0){
                printf("Error: no se puede enviar la confirmacion.\n");
                exit(-1);
        }

	memset(buf,0,sizeof(buf));
	
}


void delDir(int sd2, char *param){
	
        int ret;
	char buf[SIZE] = {'\0'};
	errno = 0;

	ret = rmdir(param);

        if (ret == -1) {
                switch (errno) {
                    case EACCES:
                        strcpy(buf,"the parent directory does not allow write");
                        break;
                    case EEXIST:
                        strcpy(buf,"pathname already exists");
                        break;
                    case ENAMETOOLONG:
                        strcpy(buf,"pathname is too long");
                        break;
		    case ENOTDIR:
			strcpy(buf,"A component of path is not a directory.");
		    default:
                        perror("rmdir");
                        exit(EXIT_FAILURE);
		}
        }
	else{
                sprintf(buf,"Directory removed");
        }

        if(write(sd2,buf,sizeof(buf)) < 0){
                printf("Error: no se puede enviar la confirmacion.\n");
                exit(-1);
        }

        memset(buf,0,sizeof(buf));

}


void operate(int sd2, int c_port){

	char oper[CMDSIZE]={'\0'}, param[BYTES]={'\0'};
	char buf[BYTES]={'\0'};
	char *token = NULL;
	
	//para el comando PORT
	struct sockaddr_in port_addr;
        int fh;
	bool act_mode = false;
	char buf_temp[BYTES]={'\0'};

	while(true){
		
		//CHEQUEAMOS LOS COMANDOS ENVIADOS POR EL CLIENTE
		if(read(sd2,buf,sizeof(buf)) < 0){
                	printf("Error: no se pudo leer el mensaje del cliente.\n");
                	perror("Error: ");
			exit(-1);
        	}
		
		strcpy(buf_temp,buf);
		buf[strcspn(buf, "\r\n")] = 0;
		token = strtok(buf, " ");
		strcpy(oper,token);

		//ELEGIMOS LA BIFURCACION EN BASE AL COMANDO RECIBIDO
		
		if(strncmp(oper, "PORT",4) == 0){

			//variables usadas para obtener los datos del PORT
        		int port[2];
			int port_dec;
		
			//variables para asociar el socket
		        int ip[4];
		        char ip_decimal[40] = {'\0'};
			
			act_mode = true;

			fh = socket(AF_INET, SOCK_STREAM, 0);
			printf("buffer: %s\n",buf_temp);
			sscanf(buf_temp,"PORT %d,%d,%d,%d,%d,%d\r\n",&ip[0],&ip[1],&ip[2],&ip[3],&port[0],&port[1]);
		        port_addr.sin_family=AF_INET;
		        sprintf(ip_decimal, "%d.%d.%d.%d", ip[0], ip[1], ip[2],ip[3]);
		        printf("IP is %s\n",ip_decimal);
		        port_addr.sin_addr.s_addr = inet_addr(ip_decimal);
      		  	port_dec = port[0]*256+port[1];
       			printf("Port is %d\n",port_dec);
			port_addr.sin_port = htons(port_dec);	
	
			//enviar mensaje de confirmacion (por canal de comandos)
			//el cliente debe leer el mensaje para comprobarlo
			memset(buf_temp,0,sizeof(buf_temp));	

			strcpy(buf_temp,MSG_200);
			if(write(sd2,buf_temp,sizeof(buf_temp)) == -1){
        			printf("Error: no se pudo enviar la confirmacion de PORT.\n");
        		        exit(-1);
        		}
	
			memset(buf_temp,0,sizeof(buf_temp));

		}
		else if(strcmp(oper, "RETR") == 0){
			
			token = strtok(NULL," ");
        		strcpy(param,token);
			token = NULL;
			retr(sd2,param,c_port,act_mode,fh,port_addr);
			act_mode = false;

		}
		else if(strcmp(oper, "CWD") == 0){
			
			token = strtok(NULL," ");
			strcpy(param,token);
			token = NULL;
			cwd(sd2,param);

		}
		else if(strcmp(oper, "NLIST") == 0){
			
			token = strtok(NULL," ");
                        strcpy(param,token);
                        token = NULL;
			nlist(sd2,param);

		}

		//CREO QUE DEBERIA PASARLE LA DIRECCION DEL CLIENTE Y NO SOLO HACERLO COMO DEFINE, YA QUE EL CLIENTE PUEDE PEDIR CREAR UNA CARPETA DENTRO DE OTRA CARPETA
		else if(strcmp(oper, "MKD") == 0){
			
			token = strtok(NULL," ");
                        strcpy(param,token);
                        token = NULL;
                        addDir(sd2,param);

		}
		else if(strcmp(oper, "RMD") == 0){
			
			token = strtok(NULL," ");
                        strcpy(param,token);
                        token = NULL;
                        delDir(sd2,param);

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
		memset(oper,0,sizeof(oper));
		memset(param,0,sizeof(param));
		memset(buf,0,sizeof(buf));
		memset(buf_temp,0,sizeof(buf_temp));
	}
	memset(oper,0,sizeof(oper));
        memset(param,0,sizeof(param));
	memset(buf,0,sizeof(buf));
	memset(buf_temp,0,sizeof(buf_temp));

}

