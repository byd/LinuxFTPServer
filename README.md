###LinuxFTPServer

A light weight ftp server. Support multiple connection and command line management.  
The program is executable, but requires more perfection and standardization.  
  
   
   
__Compile:__  
cd to the root directory, and run
> $make server 
> $make client  
> $make daemon
> $make clean 

the runnable file 'client' and 'server(ftp\_daemon)' is the executable file.   
place them wherever you want, the program uses it as its service directory.   
  
__Usage:__  
--for server--  
> \#run the daemon to make the server run as a service   
> \#the server is running on starting  
> start : start a stopped server  
> stop  : stop the running server  
> exit  : shutdown all connection and exit the program  
   
--for client--  
> dir : list files on the server  
> ls  : list local files, supporting arguments  
> cd  : change to the directory on server  
> get : download file from server  
> put : put local file onto server  
> del : delete file on server  
> bye : close the connection and exit the program   

__FILE__  
terminal.\*: control server status  
command.\* : interact with client, exchange commands   
server.\*  : server which can interact with you when debug    
daemon.\*  : make the ftp server run in daemon  
client.\*  : client actions all here  
lib/netcmd.\*: network commnads shared by both client and server  
