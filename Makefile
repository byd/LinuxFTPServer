objs = server.o msg.o terminal.o command.o netcmd.o
objc = client.o msg.o netcmd.o

server : $(objs)
	gcc -o server $(objs)

client : $(objc)
	gcc -o client $(objc)

terminal.o : terminal.c terminal.h server.h
	gcc -c terminal.c
msg.o : lib/msg.h lib/msg.c
	gcc -c lib/msg.c
netcmd.o : lib/netcmd.h lib/netcmd.c
	gcc -c lib/netcmd.c
server.o : server.c server.h
	gcc -c server.c
client.o : client.c 
	gcc -c client.c
command.o : command.c command.h
	gcc -c command.c


clean : 
	-rm $(objs) client.o
