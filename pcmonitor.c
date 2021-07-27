#define W_WIDTH 300
#define W_HEIGHT 100
#define GRAPH_COUNT (W_WIDTH / 2)
#define _NET_WM_STATE_ADD           1    // add/set property
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <string.h>
#include <cairo.h>
#include <cairo-xlib.h>

double graph_datas[W_WIDTH / 2],*cpu_usage;
long long d_eth_in,d_eth_out,d_rom_in,d_rom_out,eth_in = 0, eth_out = 0, rom_in = 0, rom_out = 0, *cpu_total, *cpu_idle;
int cpu_count = 0;
FILE *cpustat, *memstat, *netstat, *iostat;

char* getnextcol(char **d) {
	char *s = *d;
	char *e;
	while(*s && *s == ' ') {
		s++;
	}
	if(*s == '\0') {
		return NULL;
	}
	e = strchr(s,' ');
	if(e == NULL) {
		*d = NULL;
	} else {
		*e = '\0';
		*d = e + 1;
	}
	return s;
}

void getiostat() {
	char buffer[256];
	long long tr = 0,tw = 0;
	fseek(iostat,0,SEEK_SET);
	while(fgets(buffer,255,iostat)) {
		char *s = buffer;
		unsigned long long r,w;
		char b[10];
		for(int i = 0; i < 2; i++) {
			char *d = getnextcol(&s);
		}
		sscanf(s,"%s %*ld %*ld %ld %*ld %*ld %*ld %ld",&b,&r,&w);
		if(strlen(b) <= 3) {
			tr += r;
			tw += w;
		}
	}
	if(rom_in != 0 && rom_out != 0) {
		d_rom_in = (tr - rom_in) * 2;
		d_rom_out = (tw - rom_out) * 2;
	}
	rom_in = tr;
	rom_out = tw;
}
	

void getnetstat() {
	char buffer[256];
	unsigned long long tx = 0,rx = 0;
	fseek(netstat,0,SEEK_SET);
	while(fgets(buffer,255,netstat)) {
		if(strchr(buffer,':') != NULL && strstr(buffer,"lo:") == NULL) {
			char *record = strchr(buffer,':') + 1;
			for(int i = 0;i < 9;i++) {
				char *d = getnextcol(&record);
				if(i == 0) {
					rx += atoll(d);
				} else if(i == 8) {
					tx += atoll(d);
				}
			}
		}
	}
	if(eth_in != 0 || eth_out != 0) {
		d_eth_in = (rx - eth_in) * 16;
		d_eth_out = (tx - eth_out) * 16;
	}
	eth_in = rx;
	eth_out = tx;
}

double getcpustat() {
	char buffer[256];
	int c = 0;
	double totalcpuusage = 0;
	fseek(cpustat,0,SEEK_SET);
	while(fgets(buffer,255,cpustat)) {
		if(strlen(buffer) >= 4 && memcmp(buffer,"cpu",3) == 0 && buffer[3] != ' ') {
			unsigned long long u,n,s,i,io,ir,sir,cputime;
			sscanf(&buffer[5],"%ld %ld %ld %ld %ld %ld %ld",&u,&n,&s,&i,&io,&ir,&sir);
			cputime = u + n + s + i + io + ir + sir;
			if(cpu_total[c] != 0 && cpu_idle[c] != 0) {
				double dcputime = cputime - cpu_total[c];
				double dcpuidle = i - cpu_idle[c];
				cpu_usage[c] = 1.0 - (dcpuidle / dcputime);
				totalcpuusage += cpu_usage[c];
			}
			cpu_total[c] = cputime;
			cpu_idle[c] = i;
			c++;
		}
	}
	return totalcpuusage / (double)c;
}

void value_format(long v, char* r) {
	if(v  > 1000000000000) {
		snprintf(r,10,"%.2lfT",(double)v / 1000000000000.0);
	} else if(v > 1000000000) {
		snprintf(r,10,"%.2lfG",(double)v / 1000000000.0);
	} else if(v > 1000000) {
		snprintf(r,10,"%.2lfM",(double)v / 1000000.0);
	} else if(v > 1000) {
		snprintf(r,10,"%.2lfK",(double)v / 1000.0);
	} else {
		snprintf(r,10,"%d",v);
	}
}

void draw_event(cairo_t* g) {
	int i;
	char buffer[256],er[16],ew[16],dr[16],dw[16],*s;
	long long memtotal, memavail;
	double cpu_usage_avg;
	getnetstat();
	getiostat();
	cpu_usage_avg = getcpustat();
	for(i = 0; i < cpu_count;i++) {
		graph_datas[i] = cpu_usage[i];
	}
	for(i = cpu_count + 1;i < GRAPH_COUNT - 1;i++) {
		graph_datas[i] = graph_datas[i + 1];
	}
	graph_datas[GRAPH_COUNT - 1] = cpu_usage_avg;
	fseek(memstat,0,SEEK_SET);
	while(fgets(buffer,sizeof(buffer) - 1,memstat)) {
		if(memcmp(buffer,"MemTotal:",9) == 0) {
			s = buffer + 9;
			memtotal = atoll(getnextcol(&s));
		}
		if(memcmp(buffer,"MemAvailable:",13) == 0) {
			s = buffer + 13;
			memavail = atoll(getnextcol(&s));
		}
	}
	value_format(d_eth_in,er);
	value_format(d_eth_out,ew);
	value_format(d_rom_in,dr);
	value_format(d_rom_out,dw);
	cairo_set_source_rgb(g,0,0,0);
	cairo_paint(g);
	cairo_set_source_rgb(g,1.0,0.6,0.2);
	for(int i = 0; i < GRAPH_COUNT;i++) {
		int y = W_HEIGHT - ((double)W_HEIGHT * graph_datas[i]);
		cairo_rectangle(g,i * 2,y,2,W_HEIGHT - y);
	}
	cairo_fill(g);
	cairo_set_source_rgba(g,1.0,0.0,0.0,0.3);
	cairo_rectangle(g,0,0,(1.0 - ((double)memavail / (double)memtotal)) * (double)W_WIDTH,W_HEIGHT);
	cairo_fill(g);
	cairo_set_source_rgb(g,1.0,1.0,1.0);
	cairo_move_to(g,0,10);
	snprintf(buffer,100,"ETH R/W(bps): %s/%s",er,ew);
	cairo_show_text(g,buffer);
	cairo_move_to(g,0,20);
	snprintf(buffer,100,"DISK R/W(sect/s): %s/%s",dr,dw);
	cairo_show_text(g,buffer);
}

void main() {
	Display *disp;
	Window win;
	Atom WM_PROTOCOLS, WM_DELETE_WINDOW,_NET_WM_STATE_ABOVE,_NET_WM_STATE;
	cairo_t *g,*mg;
	cairo_surface_t *cr_s,*cr_ms;
	int ProgramExit = 0, timer1;
	char buffer[256];
	XSizeHints winhint;
	XClientMessageEvent xcli;
	winhint.flags = PMinSize | PMaxSize;
	winhint.min_width = W_WIDTH;
	winhint.max_width = W_WIDTH;
	winhint.min_height = W_HEIGHT;
	winhint.max_height = W_HEIGHT;
	cpustat = fopen("/proc/stat","r");
	memstat = fopen("/proc/meminfo","r");
	netstat = fopen("/proc/net/dev","r");
	iostat = fopen("/proc/diskstats","r");
	if(cpustat == NULL || memstat == NULL || netstat == NULL || iostat == NULL) {
		printf("Can't get stats.\n");
		return;
	}
	while(fgets(buffer,100,cpustat)) {
		if(strlen(buffer) >= 4 && memcmp(buffer,"cpu",3) == 0 && buffer[3] != ' ') {
			cpu_count++;
		}
	}
	cpu_total = calloc(cpu_count,sizeof(long long));
	cpu_idle = calloc(cpu_count,sizeof(long long));
	cpu_usage = calloc(cpu_count,sizeof(double));
	if(cpu_idle == NULL || cpu_total == NULL || cpu_usage == NULL) {
		printf("Insufficient memory\n");
		fclose(cpustat);
		fclose(memstat);
		fclose(iostat);
		fclose(netstat);
		return;
	}
	getcpustat();
	getnetstat();
	getiostat();
	for(int i = 0;i < GRAPH_COUNT;i++) {
		graph_datas[i] = 0.0;
	}
	cr_s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,W_WIDTH,W_HEIGHT);
	g = cairo_create(cr_s);
	disp = XOpenDisplay(NULL);
	if(disp == NULL) {
		printf("XOpenDisplay failed.\n");
		return;
	}
	WM_PROTOCOLS = XInternAtom(disp,"WM_PROTOCOLS",False);
	WM_DELETE_WINDOW = XInternAtom(disp,"WM_DELETE_WINDOW",False);
	_NET_WM_STATE_ABOVE = XInternAtom(disp, "_NET_WM_STATE_ABOVE", 1);
	_NET_WM_STATE = XInternAtom(disp,"_NET_WM_STATE",1);
	win = XCreateSimpleWindow(disp,DefaultRootWindow(disp),100,100,W_WIDTH,W_HEIGHT,1,BlackPixel(disp,DefaultScreen(disp)),WhitePixel(disp,DefaultScreen(disp)));
	memset(&xcli,0,sizeof(xcli));
	xcli.type = ClientMessage;
	xcli.window = win;
	xcli.message_type = _NET_WM_STATE;
	xcli.format = 32;
	xcli.data.l[0] = _NET_WM_STATE_ADD;
	xcli.data.l[1] = _NET_WM_STATE_ABOVE;
	XSetWMProtocols(disp,win,&WM_DELETE_WINDOW,1);
	XSetWMNormalHints(disp,win,&winhint);
	XMapWindow(disp,win);
	XSendEvent(disp,DefaultRootWindow(disp),False, SubstructureRedirectMask | SubstructureNotifyMask,(XEvent*)&xcli);
	XFlush(disp);
	cr_ms = cairo_xlib_surface_create(disp,win,DefaultVisual(disp,DefaultScreen(disp)),W_WIDTH,W_HEIGHT);
	mg = cairo_create(cr_ms);
	timer1 = 0;
	while(!ProgramExit) {
		XEvent e;
		if(XPending(disp)) {
			XNextEvent(disp,&e);
			switch(e.type) {
			case ClientMessage:
				if(e.xclient.message_type == WM_PROTOCOLS &&
					e.xclient.data.l[0] == WM_DELETE_WINDOW) {
					ProgramExit = 1;
				}
				break;
			}
		} else {
			timer1++;
			if(timer1 > 500) {
				timer1 = 0;
				draw_event(g);
				cairo_set_source_surface(mg,cr_s,0,0);
				cairo_paint(mg);
			}
			usleep(1000);
		}
	}
	XCloseDisplay(disp);
	fclose(cpustat);
	fclose(netstat);
	fclose(iostat);
	fclose(memstat);
	free(cpu_idle);
	free(cpu_total);
	free(cpu_usage);
}
