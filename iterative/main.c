#include "arguments.h"
#include "server.h"
#include "utils.h"
#include "signals.h"
#include <stdio.h>
#include <stdlib.h>     // EXIT_*
#include <string.h>
#include <unistd.h>     // for close()
#include <arpa/inet.h>  // for inet_ntoa()
#include <errno.h>
#include <libdeamon/deamon.h>  // for demonice
#include "log.h"      //for syslog
#include "config.h"   //for syslog


int main(int argc, char **argv) {
  struct arguments args;

  if (parse_arguments(argc, argv, &args) != 0)
    return EXIT_FAILURE;

  daemon_pid_file_ident = daemon_log_ident = "ftp_server";


  printf("Starting server on %s:%d\n", args.address, args.port);
  
  pid_t pid_existente;   //check for running instance
  if ((pid_existente = daemon_pid_file_is_running()) >= 0) {
    fprintf(stderr, "El demonio ya está corriendo (PID: %d)\n", pid_existente);
    return EXIT_FAILURE;
  }

  // 4. Inicializar el valor de retorno para comunicación Padre-Hijo
  if (daemon_retval_init() < 0) {
    fprintf(stderr, "Error inicializando libdaemon\n");
    return EXIT_FAILURE;
  }

  // 5. Fork inicial
    pid_t pid = daemon_fork();

    if (pid < 0) { // Error
        daemon_retval_done();
        return EXIT_FAILURE;
    } 
    
    if (pid > 0) { // Proceso Padre: Espera a que el hijo confirme éxito
        int res = daemon_retval_wait(10); 
        return res < 0 ? EXIT_FAILURE : res;
    }

    /* --- INICIO DEL PROCESO DEMONIO (Hijo) --- */

    // Cerrar streams estándar y crear archivo PID
    daemon_pid_file_create();
    log_init("ftp_server"); // Inicializa syslog

// 6. Inicializar lógica del servidor
  int listen_fd = server_init(args.address, args.port);
  if (listen_fd < 0) {
    log_write(LOG_ERR, "Error iniciando el socket en %s:%d", args.address, args.port);
    daemon_retval_send(1); // Notifica error al padre
    goto finish;
  }

  // 7. Señal de éxito al padre: El demonio ya es independiente
  daemon_retval_send(0);
  setup_signals();

  log_write(LOG_INFO, "Servidor FTP demonizado en %s:%d", args.address, args.port);
  while(1) {
    struct sockaddr_in client_addr;
    int new_socket = server_accept(listen_fd, &client_addr);

    if (new_socket < 0)
      continue;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    log_write(LOG_INFO, "Conexion aceptada desde %s:%d", client_ip, ntohs(client_addr.sin_port));

    server_loop(new_socket);

    printf("Connection from %s:%d closed\n", client_ip, ntohs(client_addr.sin_port));
    log_write(LOG_INFO, "Conexion cerrada con %s", client_ip);
  }

  finish:
    log_write(LOG_INFO, "Cerrando demonio");
    daemon_pid_file_remove();
    log_close();
    return EXIT_SUCCESS;
}
