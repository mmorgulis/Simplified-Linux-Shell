#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_LINE 4096
#define MAX_ARGS 256
#define MAX_PATH 512
#define MAX_PROMPT 32
#define MAX_VAR 100

char _path[MAX_PATH] = "/bin/:/usr/bin/"; //path di default

//Metodi per la gestione della mappa di stringhe
//contentente le variabili definite

//Definzione del tipo coppia chiave-valore
typedef struct {
	char* key;
	char* value;
} KeyValuePair;

//Definizione del tipo Mappa di stringhe
typedef struct {
	int size;
	int capacity;
	KeyValuePair* data;
} StringMap;

StringMap variables;

//Metodo per l'inizializzazione della mappa
void initializeMap(StringMap* map) {
	map->size = 0;
	map->capacity = MAX_VAR;
	map->data = (KeyValuePair*)malloc(MAX_VAR * sizeof(KeyValuePair));
}

//Metodo per aggiungere chiave-elemento all'interno della mappa
void addToMap(StringMap* map, const char* key, const char* value) {
	if (map->size < map->capacity) {
		KeyValuePair* pair = &(map->data[map->size]);
		pair->key = strdup(key);
		pair->value = strdup(value);
		map->size++;
	} else {
		puts("Error, too much variables\n");
	}
}

//Metodo per l'aggiornamento delle variabili
void changeValueForKey(StringMap* map, const char* key, const char* newValue) {
	for(int i = 0; i < map->size; ++i) {
		if(strcmp(map->data[i].key, key) == 0) {
			free(map->data[i].value);
			map->data[i].value = strdup(newValue);
			return;
		}
	}
}

//Metodo che ritorna il valore associato alla chiave
const char* getValueFromKey(const StringMap* map, const char* key) {
	for(int i = 0; i < map->size; ++i){
		if(strcmp(map->data[i].key, key) == 0) {
			return map->data[i].value;
		}
	}
	return NULL;
}

void panic(const char* msg) {
  if(errno) {
    fprintf(stderr, "PANIC: %s: %s\n\n", msg, strerror(errno));
  } else {
    fprintf(stderr, "PANIC: %s\n\n", msg);
  }
  exit(EXIT_FAILURE);
}

int prompt(char* buf, size_t buf_size, const char* str) {
  printf("%s", str);
if (fgets(buf, buf_size, stdin) == NULL) {
    return EOF;
  }
  size_t cur = -1; //current position
  do {
    cur++;
    if(buf[cur] == '\n') {
      buf[cur] = '\0';
      break;
    }
  } while(buf[cur] != '\0');
  return cur;
}

//Metodo che si occupa della definizione di variabili
void set(const char* nomeVar, const char* valueVar) {
	//il path viene inserito sia all'interno della mappa sia usato
	//come variabile globale per semplificare il codice
	if(strcmp(nomeVar, "PATH") == 0) {
 		int cur_pos = 0;
    	while(valueVar[cur_pos] != '\0') {
      		cur_pos++;
      		if(cur_pos >=  MAX_PATH - 1 && valueVar[cur_pos] != '\0') {
				fprintf(stderr, "Error: PATH string too long\n");
				return;
      		}
    	}
    if(cur_pos > 0)
      memcpy(_path, valueVar, cur_pos + 1);
  }
  if(nomeVar != NULL) {
  	if (getValueFromKey(&variables, nomeVar) == NULL) {
  		addToMap(&variables, nomeVar, valueVar);
  	} else {
  		changeValueForKey(&variables, nomeVar, valueVar);
  	}
  }
}

void path_lookup(char* abs_path, const char* rel_path) {
  char* prefix;
  char buf[MAX_PATH];
  if(abs_path == NULL || rel_path == NULL)
    panic("get_abs_path: parameter error");
  prefix = strtok(_path, ":");
  while(prefix != NULL) {
    strcpy(buf, prefix);
    strcat(buf, rel_path);
    if(access(buf, X_OK) == 0) {
      strcpy(abs_path, buf);
      return;
    }
    prefix = strtok(NULL, ":");
  }
  strcpy(abs_path, rel_path);
}

void exec_rel2abs(char** arg_list) {
  if(arg_list[0][0] == '/') {
    execv(arg_list[0], arg_list);
  } else {
    char abs_path[MAX_PATH];
    path_lookup(abs_path, arg_list[0]);
    execv(abs_path, arg_list);
  }
}

void do_redir(const char* out_path, char** arg_list, const char* mode) {
  if(out_path == NULL)
    panic("do_redir: no path");
  int pid = fork();
  if (pid > 0) {
    int wpid = wait(NULL);
    if(wpid < 0) panic("do_redir: wait");
  } else if(pid == 0) {
    // begin child code
    FILE* out = fopen(out_path, mode);
    if(out == NULL) {
      perror(out_path);
      exit(EXIT_FAILURE);
    }
    dup2(fileno(out), 1); // 1 == fileno(stdout)
    exec_rel2abs(arg_list);
    perror(arg_list[0]);
    exit(EXIT_FAILURE);
    // end child code
  } else {
    panic("do_redir: fork");
  }
}

void do_pipe(size_t pipe_pos, char** arg_list) {
  int pipefd[2];
  int pid;
  if(pipe(pipefd) < 0) panic("do_pipe: pipe");
  // left side of the pipe
  pid = fork();
  if (pid > 0) {
    int wpid = wait(NULL);
    if(wpid < 0) panic("do_pipe: wait");
  } else if(pid == 0) {
    // begin child code
    close(pipefd[0]);
    dup2(pipefd[1], 1);
    close(pipefd[1]);
    exec_rel2abs(arg_list);
    perror(arg_list[0]);
    exit(EXIT_FAILURE);
    // end child code
  } else {
    panic("do_pipe: fork");
  }
  // right side of the pipe
  pid = fork();
  if (pid > 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    int wpid = wait(NULL);
    if(wpid < 0) panic("do_pipe: wait");
  } else if(pid == 0) {
    // begin child code
    close(pipefd[1]);
    dup2(pipefd[0], 0);
    close(pipefd[0]);
    exec_rel2abs(arg_list + pipe_pos + 1);
    perror(arg_list[pipe_pos + 1]);
    exit(EXIT_FAILURE);
    // end child code
  } else {
    panic("do_pipe: fork");
  }
}

void do_exec(char** arg_list) {
  int pid = fork();
  if (pid > 0) {
    int wpid = wait(NULL);
    if(wpid < 0) panic("do_exec: wait");
  } else if(pid == 0) {
    // begin child code
    exec_rel2abs(arg_list);
    perror(arg_list[0]);
    exit(EXIT_FAILURE);
    // end child code
  } else {
    panic("do_exec: fork");
  }
}

int main(void) {
  initializeMap(&variables); //inizializzazione della mappa
  addToMap(&variables, "PATH", "/bin/:/usr/bin/");
  //aggiungo alla mappa come primo elemento il path standard
  char input_buffer[MAX_LINE];
  size_t arg_count;
  char* arg_list[MAX_ARGS];
  char prompt_string[MAX_PROMPT] = "\0";
  if(isatty(0)) {
  	strcpy(prompt_string, "dsh$ \0");
  }
  while(prompt(input_buffer, MAX_LINE, prompt_string) >= 0){
    //tokenize string
    arg_count = 0;
    arg_list[arg_count] = strtok(input_buffer, " ");
    if(arg_list[0] == NULL) {
      // nothing was spercified at the commando prompt
      continue;
    } else {
      do {
	arg_count++;
	if(arg_count > MAX_ARGS) break;
	arg_list[arg_count] = strtok (NULL, " ");
	//per riusare strtok devo mettere puntatore a null!
      }	while(arg_list[arg_count] != NULL);
    };

#if USE_DEBUG_PRINTF
    //DEBUG print tokens
    printf("DEBUG: tokens:");
    for(size_t i = 0; i < arg_count; i++) {
      printf(" %s", arg_list[i]);
    }
    puts("");
#endif
    // builtins
    if(strcmp(arg_list[0], "exit") == 0) {
      break;
    }
    //if per la gestione del set
    if(strcmp(arg_list[0], "set") == 0) {
    	set(arg_list[1], arg_list[2]);
    	continue;
    }
    {
      // check for special charachers
      size_t redir_pos = 0;
      size_t append_pos = 0;
      size_t pipe_pos = 0;
      size_t var_pos = 0;
      for(size_t i = 0; i < arg_count; i++) {
		if(strcmp(arg_list[i], ">") == 0) {
	  		redir_pos = i;
	  		break;
		}
		if(strcmp(arg_list[i], ">>") == 0) {
	  		append_pos = i;
	  		break;
		}
		if(strcmp(arg_list[i], "|") == 0) {
	  		pipe_pos = i;
	  		break;
		}
		//verifico se all'interno dei token esiste il carattere '$'
		for (size_t j = 0; j < strlen(arg_list[i]); ++j) {
			if(arg_list[i][j] ==  '$') {
				var_pos = i;
				break;
			}
		}
      }
      if(redir_pos != 0) {
		arg_list[redir_pos] = NULL;
		do_redir(arg_list[redir_pos + 1], arg_list, "w+");
      } else if (append_pos != 0) {
		arg_list[append_pos] = NULL;
		do_redir(arg_list[append_pos + 1], arg_list, "a+");
      } else if (pipe_pos != 0) {
		arg_list[pipe_pos] = NULL;
		do_pipe(pipe_pos, arg_list);
	  } else if (var_pos != 0) {
		char* var = arg_list[var_pos];
		memmove(var, var + 1, strlen(var)); //rimuovo '$'
		const char* value = getValueFromKey(&variables, var);
		// cerco all'interno della mappa delle variabili se trovo la chiave
		if (value != NULL) {
			printf("%s\n", value);
		} else {
			printf("Nessuna variable associata a %s\n", var);
		}
      } else {
		do_exec(arg_list);
      }
    }
  }
  //libero spazio di heap relativo alla mappa
  for(int i = 0; i < variables.size; ++i) {
  	free(variables.data[i].key);
  	free(variables.data[i].value);
  }
  free(variables.data);
  // puts("");
  exit(EXIT_SUCCESS);
}
