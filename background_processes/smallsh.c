#include "stdio.h"
#include "stdlib.h"
#include "signal.h"
#include "string.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "sys/wait.h"

//struct for processes
typedef struct my_child{
   int my_pid;
   struct my_child *next;
} my_child;

//gets the users input
char *getInput(){
   char *getLine = NULL;
   ssize_t buffer = 0;
   getline(&getLine, &buffer, stdin);
   return getLine;
}

//tokenizes the input from the user
char **tokenLine(char *input, int *argnum){
   int i = 0;
   char *token;

   //allows command line size of 512
   char **array = malloc(512 * sizeof(char*));

   //gets each argument and stores them individually in an array and gets the amount of arguments
   token = strtok(input, " \n");
   while(token != NULL){
      array[i] = token;
      i++;
      token = strtok(NULL, " \n");
   }

   //makes array NULL terminated
   array[i] = NULL;
   *argnum = i;
   return array;
}

//check if background process is done
void backgroundCheck(){
   int pid;
   int my_status;

   do{
      pid = waitpid(-1, &my_status, WNOHANG);

      if(pid > 0){
	 printf("Background pid %d is done: ", pid);
	 if(WIFEXITED(my_status))
	    printf("Exited value: %d\n", WEXITSTATUS(my_status));
	 else if (WIFSIGNALED(my_status) != 0)
	    printf("terminated by signal %d\n", WTERMSIG(my_status));
      }
   }while(pid > 0);
}

//inserts new process in list
void backgroundListInsert(my_child *curr, int my_pid){
   while(curr->next != NULL)
      curr = curr->next;

   curr->next = (my_child *)malloc(sizeof(my_child));
   curr = curr->next;
   curr->my_pid = my_pid;
   curr->next = NULL;
}

//fork and execute processes
int execution(struct my_child *start, int argnum, int *sigflag, int *termsig, char **arguments, int background){
   pid_t pid, wpid;
   int status;

   pid = fork();

   if(pid == 0){
      
      //restore signals to default
      struct sigaction dfl;
      dfl.sa_handler = SIG_DFL;
      dfl.sa_flags = 0;
      sigaction(SIGINT, &dfl, 0);

      //I/O redirection
      int fileDescriptor;
      int input = -1;
      int output = -1;

      int i = argnum;
      if(background == 1)
	 i = argnum - 1;

      //checks for I/O redirection
      int j;
      for(j = 0; j < i; j++){
	 if(!strcmp(arguments[j], "<"))
	    input = j;
	 else if(!strcmp(arguments[j], ">"))
	    output = j;
      }

      //Redirects input
      if(input > -1){
	 fileDescriptor = open(arguments[input + 1], O_RDONLY, 0644);

	 if(fileDescriptor == -1){
	    printf("Cannot open file");
	    exit(1);
	 }
	 else{
	    if(dup2(fileDescriptor, 0) == -1){
	       printf("Could not redirect input to file");
	       exit(1);
	    }
	    arguments[input] = NULL;
	    close(fileDescriptor);
	 }
      }

      //Redirects output
      else if(output > -1){
	 fileDescriptor = open(arguments[output + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

	 if(fileDescriptor == -1){
	    printf("Cannot open file for output redirections\n");
	    exit(1);
	 }
	 else{
	    if(dup2(fileDescriptor, 1) == -1){
	       printf("Could not redirect output\n");
	       exit(1);
	    }
	    arguments[output] = NULL;
	    close(fileDescriptor);
	 }
      }

      //redirects to /dev/null for background processes
      else if((background == 1) && (input < 0)){
         fileDescriptor = open("/dev/null", O_RDONLY, 0644);
         dup2(fileDescriptor, 0);
         close(fileDescriptor);
      }

      //executes
      if(execvp(arguments[0], arguments) == -1){
         printf("No such file or directory\n");
         exit(1);
      }
   }
   
   //forking done gone wrong
   else if(pid < 0){
      printf("Fork problems\n");
      return 1;
   }

   //parent in control
   else{
      do{
	 if(background == 1){
	    backgroundListInsert(start, pid);
	    printf("Background pid %d has begun.\n", pid);
	    fflush(stdout);
	 }
	 else
	    wpid = waitpid(pid, &status, WUNTRACED);
      }while(!WIFEXITED(status) && !WIFSIGNALED(status));

      //Termination signals and exit declarations
      if(WIFEXITED(status)){
	 *sigflag = 0;
	 return(WEXITSTATUS(status));
      }
      else if(WIFSIGNALED(status)){
	 *sigflag = 1;
	 *termsig = WTERMSIG(status);
	 return 1;
      }
   }
}

//checks the type of command and arguments
int commCheck(my_child *start, int argnum, int *sigflag, int *termsig, int *exiting, char **arguments){
   int background = 0;

   //allows commenting and blank lines
   if((arguments[0] == NULL) || !(strcmp(arguments[0], "#")))
      return 1;

   //built-in funciton for status
   else if(!strcmp(arguments[0], "status")){
      if(*sigflag == 0)
	 printf("Exit value: %d\n", *exiting);
      else
	 printf("Terminated by signal %d\n", *termsig);
      return 1;
   }

   //buit-in function for exit
   else if(!strcmp(arguments[0], "exit"))
      return 0;

   //built-in function for cd
   else if(!strcmp(arguments[0], "cd")){
      if(arguments[1] == NULL){
	 char *path;
	 path = getenv("HOME");
	 if(path != NULL)
	    chdir(path);
      }
      else{
	 if(chdir(arguments[1]) != 0){
	    printf("Directory %s does not exist\n", arguments[1]);
	    *exiting = 1;
	 }
      }
      return 1;
   }

   //checks if it is a background process
   if(!(strcmp(arguments[argnum - 1], "&"))){
      background = 1;
      arguments[argnum - 1] = NULL;
   }

   //exec() other fonctions
   *exiting = execution(start, argnum, sigflag, termsig, arguments, background);

   return 1;
}

int main(){
   my_child *start;
   my_child *curr;
   char **arguments;
   int exiting = 0;
   int sigflag = 0;
   int shellstatus, argnum, termsig;
   char *input;

   //initialize the beginning struct
   start = (my_child *)malloc(sizeof(my_child));
   curr = start;
   curr->my_pid = -1;
   curr->next = NULL;

   //As shown in class I am making the signals in the parent be ignored
   struct sigaction ign;
   ign.sa_flags = 0;
   ign.sa_handler = SIG_IGN;
   sigaction(SIGINT, &ign, NULL);

   do{
      backgroundCheck();
      
      printf(": ");
      fflush(stdout); //flushes output just like assignment says

      input = getInput();

      arguments = tokenLine(input, &argnum);

      shellstatus = commCheck(start, argnum, &sigflag, &termsig, &exiting, arguments); //checks commands and executes

      free(input);
      free(arguments);
   } while(shellstatus);

   //kill each process
   curr = start;
   while(curr != NULL){
      if(curr->my_pid != -1){
	 kill(curr->my_pid, SIGKILL);
      }
      curr = curr->next;
   }

   //frees each pointer in the list
   curr = start;
   while(curr != NULL){
      start = start->next;
      free(curr);
      curr = start;
   }

   return 0;
}
