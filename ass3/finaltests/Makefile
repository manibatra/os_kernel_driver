tests : writeless readmore readless readmore_no_block writemore writemore_no_block ioctltest ioctltestfail sculltest

writeless: writeless.c
	gcc -static writeless.c -o writeless

readless: readless.c
	gcc -static readless.c -o readless

readmore: readmore.c
	gcc -static readmore.c -o readmore

readmore_no_block: readmore_no_block.c
	gcc -static readmore_no_block.c -o readmore_no_block

writemore: writemore.c
	gcc -static writemore.c -o writemore

writemore_no_block: writemore_no_block.c
	gcc -static writemore_no_block.c -o writemore_no_block

ioctltest: ioctltest.c
	gcc -static ioctltest.c -o ioctltest

sculltest: sculltest.c
	gcc -static sculltest.c -o sculltest

ioctltestfail: ioctltestfail.c
	gcc -static ioctltestfail.c -o ioctltestfail

clean: 
	rm writeless readmore readless readmore_no_block writemore writemore_no_block ioctltest sculltest ioctltestfail

