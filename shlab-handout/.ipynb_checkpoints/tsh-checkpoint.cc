// 
// tsh - A tiny shell program with job control
// 
// Chandler de Spirlet - chde5331
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>

#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions
//

static char prompt[] = "tsh> ";
int verbose = 0;





/***********************
 * Function Prototypes
 ***********************/

/* Core Functionality */
void eval(char *cmdline);
int parseline(const char *cmdline, char **argv); 
int builtin_cmd(char **argv);
void do_bgfg(char **argv, int isBG);
void waitfg(pid_t pid);

/* Signal Handlers */
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);

/* Job Manipulation Functions */
void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int changeJobState(struct job_t *jobs, pid_t pid, int state);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
int jid2pid(struct job_t *job);
void listjobs(struct job_t *jobs);
void printJob(struct job_t *jobs, pid_t pid);

/* Helper Functions */
void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);




//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
// 

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Wrapper functions */
handler_t *Signal (int signum, handler_t *handler);
void SigEmptySet (sigset_t *mask);
void SigAddSet (sigset_t *mask, int signo);
void SigProcMask (int signo, sigset_t *mask, sigset_t *oset);
void Setpgid (pid_t pid, pid_t pgid);
void Kill (pid_t pid, int signum);

//
// main - The shell's main routine 
//
int main(int argc, char **argv) 
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h':             // print help message
      usage();
      break;
    case 'v':             // emit additional diagnostic info
      verbose = 1;
      break;
    case 'p':             // don't print a prompt
      emit_prompt = 0;  // handy for automatic testing
      break;
    default:
      usage();
    }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler); 

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
    //
    // Read command line
    //
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }

    char cmdline[MAXLINE];

    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      app_error("fgets error");
    }
    //
    // End of file? (did user type ctrl-d?)
    //
    if (feof(stdin)) {
      fflush(stdout);
      exit(0);
    }

    //
    // Evaluate command line
    //
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  } 

  exit(0); //control never reaches here
}
  
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
// 
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) 
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
  char *argv[MAXARGS];
  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
  char buff[MAXLINE]; //buffer storing the command line
  pid_t pid; //process id of current process
  int isBG; //determines whether the job should be run in fg or bg
  sigset_t mask; //mask used to block child process mask
  
  strcpy(buff, cmdline); //read contents of stdin into buff from cmdline
  
  isBG = parseline(buff, argv); //copy args and see if process should be bg or fg
  if (argv[0] == NULL){ //Do nothing if empty
    return;
  }
  
  if (!builtin_cmd(argv)){//if not built in cmd, create child process
    SigEmptySet(&mask); //create mask to block SIGCHILD
    SigAddSet(&mask, SIGCHLD); //block SIGCHILD
    SigProcMask(SIG_BLOCK, &mask, NULL); //apply masks
    
    if ((pid = fork()) == 0){ //child process to run the inputed command
      SigProcMask(SIG_UNBLOCK, &mask, NULL); //unblock SIGCHILD, SIGINT, and SIGTSTP
      Setpgid(0, 0); //reset process group of child so that it runs in its own pgrp, not from the shell
      
      if (execve(argv[0], argv, environ) < 0){ //call system to execute the command
        printf("%s: Command not found\n", argv[0]);
        exit(0);
      }
    }
    if (!isBG){ //Process is the parent, wait for fg job to finish
      if (!addjob(jobs, pid, FG, cmdline)){ //add jobs to the bg
        unix_error("added empty job");
      }
      SigProcMask(SIG_UNBLOCK, &mask, NULL); //unblock SIGCHILD
      waitfg(pid);
      return;
    } else {
      if (!addjob(jobs, pid, BG, cmdline)){
        unix_error("added empty job");
      }
      SigProcMask(SIG_UNBLOCK, &mask, NULL);
      printJob(jobs, pid);
      return;
    }
  }
}


/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need 
// to use the argv array as well to look for a job number.
//
int builtin_cmd(char **argv) 
{
  if (!strcmp(argv[0], "quit")){
    exit(0);
  } else if (!strcmp(argv[0], "jobs")){
      listjobs(jobs);
      return 1;
  } else if (!strcmp(argv[0], "fg")){
      int isBG = 0;
      do_bgfg(argv, isBG);
      return 1;
  } else if (!strcmp(argv[0], "gb")){
      int isBG = 1;
      do_bgfg(argv, isBG);
      return 1;
  } else if (!strcmp(argv[0], "&")){
      return 1;
  }
  return 0; //not a built in command
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
//
void do_bgfg(char **argv, int isBG) 
{
  struct job_t *currentJob;
  pid_t pid;
  int jid;
  int jobState;
  char bg_fg_JID_char;
  char *cmdStr = "fg";
  
  if (isBG){
    cmdStr = "bg";
  }
  
  if (argv[1] == NULL){
    printf("%s command requires PID or %%jobid argument\n", cmdStr);
    return;
  }
  
  char *bg_fg_arg = argv[1]; //check if bg/fg is valid argument
  if (bg_fg_arg[0] == '%'){ //fg/bg command
    if (!isdigit(bg_fg_arg[1])){
      printf("%s: argument must be a PID or %%jobid\n", cmdStr);
      return;
    } else { //valid jid
      bg_fg_JID_char = bg_fg_arg[1];
      jid = bg_fg_JID_char - '0'; //convert char jid to int jid
      if ((currentJob = getjobjid(jobs, jid)) == NULL){ //get job from jobs list and error checks
        printf("%s: No such job\n", argv[1]);
        return;
      }
      if ((pid = jid2pid(currentJob)) < 1){ //retrieve pid from jid and error check
        printf("(%d): No such process\n", pid);
        return;
      }
    }
  } else { //user entered pid
    if (!isdigit(bg_fg_arg[0])){ //user entered a non-numeric pid
      printf("%s: argument must be a PID or %%jobid\n", cmdStr);
      return;
    } else {
      pid = atoi(argv[1]); //cast the entered pid into an int
      if ((jid = pid2jid(pid)) == 0){ //check if pid is in jobs list
        printf("(%d): No such process\n", pid);
        return;
      }
    }
  }
  if (isBG){
    jobState = BG;
    Kill(-pid, SIGCONT); //continue all jobs
    changeJobState(jobs, pid, jobState);
    printJob(jobs, pid); //print bg job
  } else {
    jobState = FG;
    Kill(-pid, SIGCONT);
    changeJobState(jobs, pid, jobState);
    waitfg(pid); //block all processes until fg is done
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid)
{
  while (pid == fgpid(jobs)){ //while pid is equal to a fg pid, dont do anything.
    sleep(0.1);
  } //Once pid is not a fg process, stop sleeping
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//     a child job terminates (becomes a zombie), or stops because it
//     received a SIGSTOP or SIGTSTP signal. The handler reaps all
//     available zombie children, but doesn't wait for any other
//     currently running children to terminate.  
//
void sigchld_handler(int sig) //Tells us that there is a child and we need to wait
{
  pid_t pid;
  int childStatus;
  
  /* read child processes when they finish and remeve them from jobs list */
  while ((pid = waitpid(-1, &childStatus, WNOHANG | WUNTRACED)) > 0){
    if (WIFSTOPPED(childStatus)){ //if child is stopped, update status in jobs list
      if (changeJobState(jobs, pid, ST)){
        printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(childStatus));
      } else {
        unix_error("Invalid job status change target");
      }
    } else { //if child was killed, delete from job list
      if (WIFSIGNALED(childStatus)){
        printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(childStatus));
      }
      deletejob(jobs, pid);
    }
  }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenver the
//    user types ctrl-c at the keyboard.  Catch it and send it along
//    to the foreground job.  
//
void sigint_handler(int sig) 
{
  pid_t currentFGpid = fgpid(jobs); //get current fg process to send signal to.
  
  if (currentFGpid > 0){
    Kill(-currentFGpid, sig);
  }
  return;
}

/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//     the user types ctrl-z at the keyboard. Catch it and suspend the
//     foreground job by sending it a SIGTSTP.  
//
void sigtstp_handler(int sig) 
{
  pid_t currentFGpid = fgpid(jobs);
  
  if (currentFGpid > 0){
    Kill(-currentFGpid, sig); //sent SIGTSTP to every process in current pgrp
  } else {
    unix_error("Error retrieving foreground job");
  }
  return;
}

/*********************
 * End signal handlers
 *********************/


/******************
* Wrapper Functions 
*******************/

//Wrapper function for SigEmptySet system call
void SigEmptySet (sigset_t *mask){
  int status;
  if ((status = sigemptyset(mask)) < 0){ //catch error in system call
    unix_error("SigEmptySet error");
  }
}

//wrapper function for SigAddSet system call
void SigAddSet(sigset_t *mask, int signo){
  int status;
  if ((status = sigaddset(mask, signo)) < 0){
    unix_error("sigaddset error");
  }
}

//wrapper function for SigProcMask system call
void SigProcMask(int signo, sigset_t *mask, sigset_t *oset){
  int status;
  if ((status = sigprocmask(signo, mask, oset)) < 0){ //catch error in system call
    unix_error("SigProcMask error");
  }
}

//wrapper function for Setpgid system call
void Setpgid(pid_t pid, pid_t pgid){
  int status;
  if ((status = setpgid(pid, pgid)) < 0){
    unix_error("setpgid error");
  }
}

void Kill(pid_t pid, int signum){
  int status;
  if ((status = kill(pid, signum)) < 0){
    unix_error("Kill error");
  }
}

int changeJobState(struct job_t *jobs, pid_t pid, int state){

	
	struct job_t *currentJob;

	if( (currentJob = getjobpid(jobs, pid)) ){
		currentJob->state = state;
		return 1;
	}

	return 0;
}

int jid2pid(struct job_t *job) {

	int i;
	int isValid_JID = 0;
	int jid = job->jid;

	/* Check to make sure that the given jobs is */
	if (jid < 1)
		return isValid_JID;
	
	for (i = 0; i < MAXJOBS; i++)
		if (jobs[i].jid == jid)
			isValid_JID = 1;

	if (isValid_JID)
		return job->pid;

	return isValid_JID;
}

void printJob(struct job_t *jobs, pid_t pid) {
	
	struct job_t *currentJob = getjobpid(jobs, pid);

	if (currentJob->pid != 0) {

		printf("[%d] (%d) ", currentJob->jid, currentJob->pid);
		printf("%s", currentJob->cmdline);
	}
}