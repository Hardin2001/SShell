# ECS150 Project #1 Simple Shell Report

## Authors: "Yang Ye" "Shuyan Dai"


### Overall Design Idea

In order to maintain the running of the shell in a way as simple as possible, we designed 
our sshell in a modular fashion. Our *main* function is largely identical to how it was 
given to us. All we needed to add to the provided *main* function to help it utilize our 
modules was a single function call to our *MySystem* function, saving us a lot of time in 
developing and testing.


### Command Parsing

Our sshell first checks the input command to see if there is output redirection, and set 
the target output accordingly. At this point the actual command is separated from the file
and used for further parsing. Then we scan it for pipes, and send the piping information 
back with the pipeline job split into multiple commands, if applicable. Since only the last 
command in a pipeline gets to redirect its output, we don't need to deal with redirection  
other than what we have done above. If there are no pipes, we will split the command 
and put the command name and different arguments into an argument array.  


### Built-in Command

Our sshell first looks for built-in command after parsing. As per the project requirement, 
we assume that built-in command cannot be called with pipes. When there are pipes, 
the program does not check for built-in command. 

If built-in command is detected, we will directly execute it without forking. The 
implementation of *cd* and *pwd* only takes a function call to the system, while *sls* is 
more complex, since we had to manually filter out the hidden files by picking out all the 
files with their names starting with a dot ('.') when using the *scandir* function. For the 
convenience of implementation, the *exit* command was kept in its original place in the 
*main* function. When no built-in command is no found, we send the command to the 
*Exec* function explained below for forked execution. 


### Execution

The function *Exec* was built to execute external command with or without piping, since 
it takes the desired IO file descriptors as two of its parameters. As the process forks, the 
child will set the IO targets as required in the parameters, and the parent will collect its 
child's return status and send it back for printing. The function also supports the  
identification of the last command in a pipeline through its parameter. 


### Pipes

If we know from previous parsing that there exist pipes in the command, we further 
parse and execute pipeline commands while setting up piping. 

_Here is a overview of the looping of the pipeline process:_

A new pipe is created between each two pipeline commands. The first command reads 
*stdin* and writes to the read end of a new pipe - we can call it *pipe 1*. The next 
command reads from *pipe 1* and writes to another new pipe - *pipe 2*. The following 
command will read from the previous pipe *pipe 2* and write to a new pipe *pipe 3*, and 
so on. The intermediate commands always read from the previously written pipe and 
write to a new pipe. Finally, The last command will read from the *n*th pipe, where *n* is 
the total number of pipes, and write to *stdout* or the redirection target specified. 

In order to save space and speed up, the "new" pipes are actually built in-place with the 
help of buffering. The abstraction of new pipes helps simplify the function. 


### Redirection

As mentioned in the parsing section, the output redirection is handled relatively early in 
the program, so the stdout of everything else will be redirected by default and we don't 
have to worry about it later. 

At the end of command execution, we revert *stdin* and *stdout* to their original setting 
saved before redirection and make sure everything is back to normal. 


### Testing

In order to keep the code efficient, we tested each function separately immediately after
its completion to ensure progress. Our modular design offered great simplicity and 
flexibility with out testing process. 

After feeding the test input to our sshell, we could see if it was able to take advantage of 
the correct system calls and generate the expected output by comparing the output with 
that of the provided reference program sshell_ref, visually or using the "diff" command. 

After the completion and testing of all functions, we tested our finalized sshell on CSIF to 
verify that all functions could cooperate correctly without any unexpected behavior. 
Beside the given tests, we also came up with our own test cases, which helped capture 
many hidden bugs.


### Sources Referenced

Due to the requirement of this project, there was no outside code used. We only 
referenced online C language guides and related question posts to check whether we 
had used the functions from C libraries properly.
