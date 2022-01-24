all: make_clientA make_clientB make_serverC make_serverT make_serverS make_serverP

make_clientA: clientA.c
	@gcc -o clientA clientA.c

make_clientB: clientB.c
	@gcc -o clientB clientB.c

make_serverC: central.c
	@gcc -o serverC central.c

make_serverT: serverT.c
	@gcc -o serverT serverT.c

make_serverS: serverS.c
	@gcc -o serverS serverS.c

make_serverP: serverP.c
	@gcc -o serverP serverP.c