// to be included in window.c for testing
static void garglktst_send_cmd(int character){
	static char cmd[256];
	if(0==fgets(cmd, sizeof(cmd), garglktstctx.inpf)){
		if(garglktstctx.eofexit){
			if(garglktstctx.outf){
				fprintf(garglktstctx.outf, "\n");
				fflush(garglktstctx.outf);
				}
			exit(0);
			}
		return;
		}
	if(character){
		if('\n'==cmd[0]){
			gli_input_handle_key(keycode_Return);
			}
		else{
			gli_input_handle_key(cmd[0]);
			}
		return;
		}
	int len=strlen(cmd);
	if(len>0 && '\n'==cmd[len-1]) cmd[len-1]='\0';
	char *s=cmd;
	for(;*s;s++){
		gli_input_handle_key(*s);
		}
	gli_input_handle_key(keycode_Return);
	sleep(garglktstctx.sleep);
	}
