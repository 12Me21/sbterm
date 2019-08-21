void modem_init(int);
void exit_pty(int);

typedef unsigned char Sample;

void init_esc(void);
char proc_esc(char,void(*callback)(char*,int,void*),void*);

//modem API:
// - call modem_init first
// - call modem_send to write data to modem buffer
// - modem_send inserts sync signals, too
// - modem uses threaded main loop so it'll just read from the buffer and whatever
//  (how THE FUCK to make sure you don't read/write at the same time???)

//alternative:
// - call modem_bytes, pass this to read()
// - pass the buffer/actual number of read bytes to modem_send
// - modem_send sends the data
// - then you call modem_pad, which fills the rest of requested space with idle

//alternative 2:
// - audio write callback, calls nonblocking read on stdout, and fuckin, modulates it right away
// - then once it runs out of data, it sends sync idles to fill space.
