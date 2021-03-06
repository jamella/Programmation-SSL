//SSL-Server.c
/*
    cc -c Server2.c `pkg-config openssl --cflags`
    cc -o serv2 Server2.o `pkg-config openssl --libs`
    ./serv2 5000
*/

#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/des.h"
#include "Attack.h"
 
#define FAIL    -1

static DES_cblock ivsetup;
static DES_key_schedule key;
 
int OpenListener(int port)
{   int sd;
    struct sockaddr_in addr;
 
    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
        perror("can't bind port");
        abort();
    }
    if ( listen(sd, 10) != 0 )
    {
        perror("Can't configure listening port");
        abort();
    }
    return sd;
}
 
int isRoot()
{
    if (getuid() != 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
 
}
SSL_CTX* InitServerCTX(void)
{   SSL_METHOD *method;
    SSL_CTX *ctx;
 
    OpenSSL_add_all_algorithms();  /* load & register all cryptos, etc. */
    SSL_load_error_strings();   /* load all error messages */
    method = SSLv3_server_method();  /* create new server-method instance */
    ctx = SSL_CTX_new(method);   /* create new context from method */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}
 
void LoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
}
 
void ShowCerts(SSL* ssl)
{   X509 *cert;
    char *line;
 
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(cert);
    }
    else
        printf("No certificates.\n");
}

// fonction callback à ajouter
int verify_callback (int ok, X509_STORE_CTX *store)
{
  int depth = X509_STORE_CTX_get_error_depth(store);
  X509 *cert = X509_STORE_CTX_get_current_cert(store);
  int err = X509_STORE_CTX_get_error(store);
 
  if(depth > 0) return ok; // just check server certif IP (at depth 0), else preverify "ok" is enough...
 
  printf("+++++ check peer certificate +++++\n");
  printf(" * preverify ok = %d\n", ok); 
  printf(" * chain depth = %d\n", depth); 
  printf(" * error code %i (%s)\n", err, X509_verify_cert_error_string(err));
  char data[256];
  X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
  printf(" * issuer = %s\n", data);
  X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
  printf(" * subject = %s\n", data);
  char * certifip = data+4;
   printf(" * certificate client IP = %s\n", certifip);
  //char * serverip = inet_ntoa(addr.sin_addr);
   char * serverip = "127.0.0.1";
   printf(" * client IP = %s\n", serverip); 

  if (ok) {      
    if(strcmp(certifip,serverip) == 0) { 
      printf("SUCCESS: certificate IP (%s) matches server IP (%s)!\n", certifip, serverip);   
      return 1; // continue verification
    }
    else {
      printf("FAILURE: certificate IP (%s) does not match server IP (%s)!\n", certifip, serverip);   
      return 0; // stop verification
    }
  }
 
  return 0; // stop verification
}
 
void Servlet(SSL* ssl) /* Serve the connection -- threadable */
{   char buf[1024];
    char reply[1024];
    int sd, bytes;
    char* decrypted;
    const char* HTMLecho="<html><body><pre>%s</pre></body></html>\n\n";

 
    if ( SSL_accept(ssl) == FAIL ) {    /* do SSL-protocol accept */
        ERR_print_errors_fp(stderr);
    }
    else
    {
        ShowCerts(ssl);        /* get any certificates */
        bytes = SSL_read(ssl, buf, sizeof(buf));
	
        if ( bytes > 0 )
        {
            buf[bytes] = 0;
            printf("Client msg: \"%s\"\n", buf);
            sprintf(reply, HTMLecho, buf);  
            SSL_write(ssl, reply, strlen(reply)); 
        }
        else
	ERR_print_errors_fp(stderr);

	//********************************************************
	
	// ****** Test read-write ssl ********
	/*memset(buf, 0, 1024);
	bytes = SSL_read(ssl, buf, sizeof(buf));
        if ( bytes > 0 )
        {
            buf[bytes] = 0;
	    
	    decrypted = malloc(sizeof(buf));
	    memcpy(decrypted, Decrypt_DES(buf,bytes), bytes);
            printf("\n\nClient Decrypted: \"%s\"\n", decrypted);
            //sprintf(reply, HTMLecho, buf);
            SSL_write(ssl, "VALIDE", 6);
        }
        else
	ERR_print_errors_fp(stderr);*/

	// ******* Test recherche cookie *********
	/*int i;
	int tmp;
        for (i = 0; i < 8; ++i)
        {
	  tmp = 0;
            while(tmp == 0){
	      bytes = SSL_read(ssl, buf, sizeof(buf));
	      buf[bytes] = 0;
	      decrypted = malloc(sizeof(char)*bytes);
	      
	      memcpy(decrypted, Decrypt_DES(buf,bytes), bytes);
	      if(decrypted[bytes-1] != '7')
                {
		  SSL_write(ssl, "INVALIDE", 8);
                }
	      else
                {
		  SSL_write(ssl, "VALIDE", 6);
		  tmp = 1;
                }
	    }
	    }*/

	// ****** Test recherche octet ******
	

        //********************************************************
    }
    sd = SSL_get_fd(ssl);       /* get socket connection */
    SSL_free(ssl);         /* release SSL state */
    close(sd);          /* close connection */
}
 
int main(int count, char *strings[])
{   SSL_CTX *ctx;
    int server;
    char *portnum;
 
    /*if(!isRoot())
    {
        printf("This program must be run as root/sudo user!!");
        exit(0);
    }*/
    if ( count != 2 )
    {
        printf("Usage: %s <portnum>\n", strings[0]);
        exit(0);
    }
    SSL_library_init();
 
    portnum = strings[1];
    ctx = InitServerCTX();        /* initialize SSL */
    LoadCertificates(ctx, "Serv.crt", "Serv.key"); /* load certs */
    server = OpenListener(atoi(portnum));    /* create server socket */
    while (1)
    {   struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        SSL *ssl;
 
        // code à ajouter pour vérifier le certificat du serveur...
        SSL_CTX_load_verify_locations (ctx, "ca.crt",0);        
        SSL_CTX_set_verify (ctx, SSL_VERIFY_PEER, verify_callback);

        int client = accept(server, (struct sockaddr*)&addr, &len);  /* accept connection as usual */

        printf("Connection: %s:%d\n",inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        ssl = SSL_new(ctx);              /* get new SSL state with context */
        SSL_set_fd(ssl, client);      /* set connection socket to SSL state */
        Servlet(ssl);         /* service connection */
    }
    close(server);          /* close server socket */
    SSL_CTX_free(ctx);         /* release context */
}
