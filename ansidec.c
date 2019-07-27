#include <ctype.h>
#include <stdio.h>

#define DEF_FCOL 0
#define DEF_BCOL 15

#define CSQ_SETF "\006",1
#define CSQ_SETB "\013",1
#define CSQ_REV "\034071",4
#define CSQ_INVIS "\034074",4
#define CSQ_EL "\003",1
#define CSQ_EL1 "\034020",4
//these aren't used by curses: implemented for this specifically
#define CSQ_REV_OFF "\034000",4
#define CSQ_INVIS_OFF "\034001",4
#define CSQ_BOLD "\034002",4
#define CSQ_BOLD_OFF "\034003",4

int args[100];
int argi;
enum {start,csi_start,csi_normal,finished} state;
char prearg;

//start processing escape sequence (call when ESC is encountered)
void init_esc(){
	argi=0;
	state=start;
	prearg=0;
}

//ANSI term state:
int fcol,bcol,bold=0,reverse=0,hidden=0;

void runcsi(int *args, int argi, char cmd, char prearg, void(*callback)(char*,int)){
	int i=0;
	int changefcol=0,changebcol=0,changehidden=0,changereverse=0,changebold=0;
	int arg;
	while(i<=argi){
		arg=args[i++];
		switch(prearg){
		case '?':
			//probably nothing important?
			switch(cmd){
			case 'h':
				switch(arg){
				case 1000: break;
				default:
					printf("unknown CSI ? high %d\r\n", arg);
					goto leave;
				}
				break;
			case 'l':
				switch(arg){
				case 1000: break;
				default:
					printf("unknown CSI ? low %d\r\n", arg);
					goto leave;
				}
				break;
			default:
				//unknown command!
				printf("unknown CSI ? xterm code: %c\r\n",cmd);
				goto leave;
			}
			break;
		case '>':
			//probably nothing important
			switch(cmd){
			default:
				printf("Unknown: CSI > ... %c\r\n", cmd);
				goto leave;
			}
			break;
		case 0: // ESC [ ... c
			switch(cmd){
			case 'm':;
				switch(arg){
				case 0:
					fcol=DEF_FCOL;
					bcol=DEF_BCOL;
					bold=0;
					reverse=0;
					hidden=0;
					changebold=changefcol=changebcol=changehidden=changereverse=1;
					break;
				case 1:
					bold=1;
					changebold=1;
					break;
				case 2:
				case 21:
				case 22:
					bold=0;
					changebold=1;
					break;
				case 7:
					reverse=1;
					changereverse=1;
					break;
				case 8:
					hidden=1;
					changehidden=1;
					break;
				case 27:
					reverse=0;
					changehidden=1;
					break;
				case 28:
					hidden=0;
					changehidden=1;
					break;
				case 30 ... 37:
					fcol=arg-30;
					changefcol=1;
					break;
				case 38:
					//set fg col
					changefcol=1;
					break;
				case 39:
					fcol=DEF_FCOL;
					changefcol=1;
					break;
				case 40 ... 47:
					bcol=arg-40;
					changebcol=1;
					break;
				case 48:
					//set bg col
					changebcol=1;
					break;
				case 49:
					bcol=DEF_BCOL;
					changebcol=1;
					break;
				case 90 ... 97:
					fcol=arg-90+8;
					changefcol=1;
					break;
				case 100 ... 107:
					bcol=arg-100+8;
					changefcol=1;
					break;
				default:
					printf("unknown CSI m control: %d\n\r", arg);
					goto leave;
				}
				break;
			case 'K':
				switch(arg){
				case 0: callback(CSQ_EL); break;
				case 1: callback(CSQ_EL1); break;
				default:
					printf("CSI %d ... K", arg);
					goto leave;
				}
				break;
			default:
				//unknown command!
				printf("unknow CSI command: %c\n\r",cmd);
				goto leave;
			}
			break;
		default:
			printf("unknown pre-arg: %c\r\n", prearg);
			goto leave;
		}
	}
leave:;
	// Now process all the colors and shit
	char argout[2];
	if(changefcol){
		callback(CSQ_SETF);
		argout[0]=fcol+' ';
		callback(argout,1);
	}
	if(changebcol){
		callback(CSQ_SETB);
		argout[0]=' '+bcol;
		callback(argout,1);
	}
	if(changereverse){
		if(reverse)
			callback(CSQ_REV);
		else
			callback(CSQ_REV_OFF);
	}
	if(changehidden){
		if(hidden)
			callback(CSQ_INVIS);
		else
			callback(CSQ_INVIS_OFF);
	}
	if(changebold){
		if(bold)
			callback(CSQ_BOLD);
		else
			callback(CSQ_BOLD_OFF);
	}
}

//process 1 char of escape sequence
//if sequence is not finished, returns 0
//otherwise returns 1
//passes sbterm escape sequences to callback function
char proc_esc(char c,void(*callback)(char*,int)){
	switch(state){
		//ESC _
	case start:
		switch(c){
		case '[': //ESC [
			state=csi_start;
			args[0]=0;
			break;
		default: //ESC <invalid>
			state=finished;
		}
		break;
		//ESC [ _
	case csi_start:
		state=csi_normal;
		if(c=='?' || c =='>'){
			prearg=c;
			break;
		}
		//fallthrough!
		//ESC [ ... _
	case csi_normal:
		if(isdigit(c)){
			args[argi]*=10;
			args[argi]+=c-'0';
		}else if(c==';'){
			argi++;
			args[argi]=0;
		}else{
			runcsi(args,argi,c,prearg,callback);
			state=finished;
			break;
		}
		break;
		//function should not have been called with this state!!!
	case finished:;
	}
	return state==finished;
}
