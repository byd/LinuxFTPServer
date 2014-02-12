objs = server.o msg.o terminal.o command.o
objc = client.o msg.o

server : $(objs)
	gcc -o server $(objs)

client : $(objc)
	gcc -o client $(objc)

terminal.o : terminal.c terminal.h server.h
	gcc -c terminal.c
msg.o : lib/msg.h lib/msg.c
	gcc -c lib/msg.c
server.o : server.c server.h
	gcc -c server.c
client.o : client.c 
	gcc -c client.c
command.o : command.c command.h
	gcc -c command.c


clean : 
	-rm $(objs) $(objc)
