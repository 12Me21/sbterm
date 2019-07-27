void modem_send(char * buffer, int length);
void modem_init(void);
void modem_sync(void);
void modem_end(void);

void init_esc(void);
char proc_esc(char c,void(*callback)(char*,int));
