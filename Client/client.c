/* CSD 331 Computer Networks Lab, Fall 2019
Client Code
 Team: Devshree Patel(075), Muskan Matwani(027), Ratnam Parikh(036), Yesha Shastri(035)
 */

//for compile:  gcc `pkg-config gtk+-2.0 --cflags` client.c -o client `pkg-config gtk+-2.0 --libs`
// 		./client 127.0.0.1

/* Including all the header files required for socket programming */
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

/*Defining all the variables used recurrently in the program*/
#define BUF_MAX_SIZE 256
#define multi_addressESS "239.192.1.10"
#define SERVER_PORT 5432
#define BUF_SIZE 4096
#define BUF_SIZE_SMALL 256

//Defining structure for station info
typedef struct station_info_t 
{
	//uint8_t type;   // = 13;
	uint8_t station_number;
	uint8_t station_name_size;
	char station_name[BUF_MAX_SIZE];
	uint32_t multicast_address;
	uint16_t data_port;
	uint16_t info_port;
	uint32_t bit_rate;
} station_info;

//Defining structure for station info request
typedef struct station_info_request_t {
	uint8_t type;   // = 1;
} station_info_request;

//Init Function for station_info_request_t
station_info_request initStationInfoRequest(station_info_request *sir)
{
	sir->type = 1;
	return *sir;
}

//Defining structure for site info
typedef struct site_info_t 
{ 
	uint8_t type;// = 10;
	uint8_t site_name_size;
	char site_name[BUF_MAX_SIZE];
	uint8_t site_desc_size;
	char site_desc[BUF_MAX_SIZE];
	uint8_t station_count;
	station_info station_list[BUF_MAX_SIZE];
} site_info;

//Init Function for station_info
site_info initSiteInfo(site_info *si)
{
	si->type = 10;	
	return *si;
}

// station not found structure
typedef struct station_not_found_t
{
	uint8_t type;  // = 11;
	uint8_t station_number;
} station_not_found;

//Defining structure for station not found
station_not_found initStationNotFound(station_not_found *snf)
{
	snf->type = 11;
	return *snf;
}

//Defining structure for song info
typedef struct song_info_t 
{
	uint8_t type;   // = 12;
	uint8_t song_name_size;
	char song_name[BUF_MAX_SIZE];
	uint16_t remaining_time_in_sec;
	uint8_t next_song_name_size;
	char next_song_name[BUF_MAX_SIZE];
} song_info;

//Init Function for song_info
song_info initSongInfo(song_info *si)
{
	si->type = 12;
	return *si;
}

//Declaring Variables
int typeChange = 0;
int TotalNStations;
station_info stations[16];

int curVLCPid = 0;
int stationNow = 0;
int count = 0;

int argC;
int forceClose = 0;
void ReceiveSongs(void*);
pthread_t recvSongsPID;

// Assiging current cur_status to 'run'
char cur_status = 'r';

// Function to start the ReceiveSongs thread to receive multicast messages (streaming songs)
void runRadio(char* argv[])
{
    pthread_create(&recvSongsPID, NULL, &ReceiveSongs, argv);

    // The pthread_join() function blocks the calling thread until the thread with identifier equal to the 'recvSongsPID' i.e. first argument of the funtion terminates.
    pthread_join(recvSongsPID, NULL);
}

// Function to clean the temporary files
void cleanFiles()
{
    system("rm tempSong*");
}

// Function to kill the current running process
void* closeFunction(void* args)
{
    char pidC[10];  
    char cmd[256];
    
    // clearing the 'cmd' and 'pidC' variables
    memset(cmd, '\0', 256);
    memset(pidC, '\0', 10);
    
    strcpy(cmd, "kill ");
    
    sprintf(pidC, "%d", curVLCPid);

    // Appending the pidC after the "kill " string in the cmd variable
    memcpy(&cmd[strlen("kill ")], pidC, strlen(pidC));
    
    system(cmd);
    return NULL;
   
}

// Function to receive the station list information and displaying it
void StationListReceive(char * argv[])
{
    int sT; 
    struct sockaddr_in s_addr;
    char* SERVER_ADDRESS;
    SERVER_ADDRESS = argv[1];
    
    // TCP socket creation 
    if ((sT = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("receiver: socket");
        exit(1);
    }

    // Initializing address data structure  
    bzero((char *) &s_addr, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);;
    s_addr.sin_port = htons(SERVER_PORT);
    
    // connecting the socket with server
    int st = -1;
    while(st < 0)
    {
        st = connect(sT, (struct sockaddr *) &s_addr, sizeof(s_addr));    
    } 
    
    char buf[BUF_SIZE_SMALL];
    int rBytes = BUF_SIZE_SMALL;
    
    send(sT, "name.txt\n", 9, 0);
    
    uint32_t NStations = 0;

    read(sT, &NStations, sizeof(uint32_t));
    NStations = ntohl(NStations);
    TotalNStations = NStations;
    
    // Total number of stations
    printf("No. of Stations : %d\n", NStations);
    if(NStations > 16)
    {
        printf("Too many stations!!!\n");
        exit(0);
    }
    
    int i=0;
    station_info* si = malloc(sizeof(station_info));
    
    // Printing the station information
    for(i=0;i<NStations;i++)
    {
        read(sT, si, sizeof(station_info));
	printf("---- STATION INFO %d ------\n", i);
        printf("Station No. %hu\n", si->station_number);
	printf("Station multicast Port : %d\n\n", ntohs(si->data_port));
        printf("Station Name : %s\n", si->station_name);
        
        memcpy(&stations[i], si, sizeof(station_info));
    }

    close(sT);
}

// Function to create a UDP packet and receiving multicast messages (songs) streaming on the user specified station
void ReceiveSongs(void* args)
{
    printf("\nReceiving Songs \n");
    char** argv = (char**)args;
    
    int s;                  
    struct sockaddr_in sin; 
    char *if_name;          
    struct ifreq ifr;       
    char buf[BUF_SIZE];
    int len;

    // Multicast Related Information
    char *multi_address;               
    struct ip_mreq mcast_req;       
    struct sockaddr_in mcast_saddr; 
    socklen_t mcast_saddr_len;
    
    if(argC == 3) {
        if_name = argv[2];
    }
    else
        if_name = "wlan0";
    
    
    // UDP socket for multicating messages
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("receiver: socket");
        exit(1);
    }
    
    // Setting unique port for different stations
    int mc_port;

    if(stationNow > TotalNStations)
    {
	// setting default first station when the station number is not valid
        printf("No such station exists. Reverting to default station\n");
        mc_port = 2300;
        stationNow = 0;

    } else {
	
	// setting the required station port if the station is valid
        mc_port = ntohs(stations[stationNow].data_port);
    }
    
    // build address data structure 
    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(mc_port);
    
    
    // Use the interface specified 
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name , if_name, sizeof(if_name)-1);
    
    if ((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&ifr, sizeof(ifr))) < 0)
    {
        perror("receiver: setsockopt() error");
        close(s);
        exit(1);
    }
    
    // bind the socket 
    if ((bind(s, (struct sockaddr *) &sin, sizeof(sin))) < 0) 
    {
        perror("receiver: bind()");
        exit(1);
    }
    
    /* Multicast specific code follows */
    
    // build IGMP join message structure 
   // joining the multicast group where all the stations reside on different ports
    mcast_req.imr_multiaddr.s_addr = inet_addr(multi_addressESS);
    mcast_req.imr_interface.s_addr = htonl(INADDR_ANY);
    
    // send multicast join message 
    if ((setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*) &mcast_req, sizeof(mcast_req))) < 0) 
    {
        perror("mcast join receive: setsockopt()");
        exit(1);
    }
    
    // receiving multicast messages 
    FILE* fp;
    int counter = 0;
    count=0;

    printf("\nLets Hear the song!\n\n");

    song_info* songInfo = (song_info*) malloc(sizeof(song_info));
    char* tempSongs[2];
    tempSongs[0] = "tempSong1.mp3";
    tempSongs[1] = "tempSong2.mp3";
    
    int typeChange = 0, cur=0;
    char* tempSong = tempSongs[cur];

    while(1) 
    {
        if(typeChange) 
	{
            tempSong = tempSongs[1 ^ cur];
        }
        
        // Reset sender structure
        memset(&mcast_saddr, 0, sizeof(mcast_saddr));
        mcast_saddr_len = sizeof(mcast_saddr);
        
        // Clear buffer and receive 
        memset(buf, 0, sizeof(buf));
        if ((len = recvfrom(s, buf, BUF_SIZE, 0, (struct sockaddr*)&mcast_saddr, &mcast_saddr_len)) < 0) 
	{
            perror("receiver: recvfrom()");
            exit(1);
        } else {
            count = count + len;
       
            if(count >= 387000) 
	    {
         	// Detaching the thread 
		printf("\nSongs streaming!\n");
                pthread_detach(pthread_self());
                pthread_exit(NULL);
                
            }
        }
        
        // Displaying song information
        uint8_t tempType;
        if(len == sizeof(song_info)) 
	{
            printf("Len = %d. Checking if songInfo...\n", len);
            memcpy(songInfo, buf, len);

            printf("type of Song info = %hu\n", songInfo->type);
            tempType = (uint8_t)songInfo->type;
            
	    // type = 12 means song info structure
            if(tempType == 12)
	    {
		// printing current song and next song name
                printf("Current Song : %s\n", songInfo->song_name);
                printf("Next Song : %s\n", songInfo->next_song_name);
                continue;
            }
            else {
                printf("Not here!\n");
	    }
        }
        
        if(counter++ == 10)
	{
            curVLCPid = fork();
            if(curVLCPid == 0)
	    {
		// execlp replaces the calling process image with a new process image. This has the effect of running a new program with 			the process ID of the calling process
                execlp("/usr/bin/cvlc", "cvlc", tempSong, (char*) NULL);

		// first arg :  /usr/bin/cvlc = path to new process image
		// arg 2 to NULL : list of arguments passed to new process image
		// second arg: cvlc = name of the executable file for the new process image
		
            }
        }
        
        fp = fopen(tempSong, "ab+");
        fwrite(buf, len, 1, fp);
        fclose(fp);
        
        // Add code to send multicast leave request 
        if(forceClose == 1)
            break;
    }
    
    close(s);
    forceClose = 0;
    fp = fopen(tempSong, "wb");
    fclose(fp);
}

// Action listener for Running the Radio on the current station
void clicked_button(GtkWidget *widget, gpointer data, char * argv[]) 
{
    printf("Running\n");
    runRadio(argv);
    cur_status = 'r';
}

// Action listener for Pausing the Radio
void clicked_button1(GtkWidget *widget, gpointer data) 
{
    printf("\n\nPausing\n");
    closeFunction(NULL);
    //pthread_cancel(recvSongsPID);
    forceClose = 1;
    //cleanFiles();
    cur_status = 'p';
}

// Action listener for Stopping the radio
void clicked_button3(GtkWidget *widget, gpointer data) 
{
    g_print("\n\nExiting! \n\n");
    closeFunction(NULL);
    forceClose = 1;
    cleanFiles();
    exit(0);
}

// Action listener for station 1
void clicked_station_1(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Station 1 tuning!!\n");
	closeFunction(NULL);
        forceClose = 1;
        cleanFiles();
        int station=1;
        stationNow = station - 1;
        cur_status = 'c';
	//gtk_window_close(widget);
	
}

// Action listener for station 2
void clicked_station_2(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Station 2 tuning!!\n");
	closeFunction(NULL);
        forceClose = 1;
        cleanFiles();
        int station=2;
        stationNow = station - 1;
        cur_status = 'c';
}

// Action listener for station 3
void clicked_station_3(GtkWidget *widget, gpointer data, char * argv[]) 
{
	g_print("Station 3 tuning!!\n");
	closeFunction(NULL);
        forceClose = 1;
        cleanFiles();
        int station=3;
        stationNow = station - 1;
        cur_status = 'c';
}

// Action listener for displaying station list
void clicked_button2(char* argv[])
{
    // Initializing Variables for GUI station list window
    GtkWidget *window;
    GtkWidget *halign;
    GtkWidget *vbox;
    GtkWidget *btn;
    GtkWidget *btn1;
    GtkWidget *btn2;
    GtkWidget *btn3;
    GdkColor color;
    GdkColor color1;
    GdkColor color2;
    GdkColor color3;
    GtkWidget *frame;
    GtkWidget *label;

    int argc = 1;
    gtk_init(&argc, &argv);
    char o;
    char lo = 'r';
    int station, f=1;
    
    // Creation of the Station List GUI Window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "INTERNET RADIO");
    gtk_window_set_default_size(GTK_WINDOW(window), 230, 150);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    vbox = gtk_vbox_new(TRUE,1);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Header label for the Station List Window
    frame = gtk_frame_new ("");
    label = gtk_label_new ("Welcome to Radio Rocky!!!");
  
    gtk_container_add (GTK_CONTAINER (frame), label);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);    
    
    //Assigning labels to the 3 buttons corresponding to 3 stations
    btn = gtk_button_new_with_label("Bollywood Masti");
    btn1 = gtk_button_new_with_label("Hollywood Mix");
    btn2 = gtk_button_new_with_label("Love is in the air");
    
    // Setting button's length and width
    gtk_widget_set_size_request(btn, 70, 30);
    gtk_widget_set_size_request(btn1, 70, 30);
    gtk_widget_set_size_request(btn2, 70, 30);
    
    // gtk Box pack widgets (buttons) into a GtkBox from start to end
    gtk_box_pack_start(GTK_BOX(vbox), btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn2, TRUE, TRUE, 0);

    // Assigning colors to variables
    gdk_color_parse ("yellow", &color);
    gdk_color_parse ("light blue", &color1);
    gdk_color_parse ("light green", &color2);
    gdk_color_parse ("red", &color3);

    // Adding colour to the respective buttons
    gtk_widget_modify_bg (GTK_WIDGET(btn), GTK_STATE_NORMAL, &color);
    gtk_widget_modify_bg (GTK_WIDGET(btn1), GTK_STATE_NORMAL, &color1);
    gtk_widget_modify_bg (GTK_WIDGET(btn2), GTK_STATE_NORMAL, &color2);  
    gtk_widget_modify_text ( GTK_WIDGET(label), GTK_STATE_NORMAL, &color3);      

    // Conecting buttons to their respective station listeners
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(clicked_station_1), argv);
    g_signal_connect(G_OBJECT(btn1), "clicked", G_CALLBACK(clicked_station_2), argv);
    g_signal_connect(G_OBJECT(btn2), "clicked", G_CALLBACK(clicked_station_3), argv);

    g_signal_connect(G_OBJECT(window), "destroy",G_CALLBACK(gtk_main_quit), G_OBJECT(window));
    
    // Displaying the Window
    gtk_widget_show_all(window);
    gtk_main();
}

// MAIN FUNCTION
int main(int argc, char * argv[])
{
    argC = argc;
   
    // Receiving and printing Station List coming from server
    StationListReceive(argv);

    // Initializing Variables for GUI main window
    GtkWidget *window;
    GtkWidget *halign;
    GtkWidget *vbox;
    GtkWidget *btn;
    GtkWidget *btn1;
    GtkWidget *btn2;
    GtkWidget *btn3;
    GdkColor color;
    GdkColor color1;
    GdkColor color2;
    GdkColor color3;

    gtk_init(&argc, &argv);

    printf("\nIn Main Function\n");
    char o;
    char lo = 'r';
    int station, f=1;
    
     // Assigning colors to variables
    gdk_color_parse ("yellow", &color);
    gdk_color_parse ("light blue", &color1);
    gdk_color_parse ("light green", &color2);
    gdk_color_parse ("light yellow", &color3);

    // Creation of the Station List GUI Window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "INTERNET RADIO");
    gtk_window_set_default_size(GTK_WINDOW(window), 230, 150);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    vbox=gtk_vbox_new(TRUE,1);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    //Assigning labels to the 4 buttons corresponding to 4 options given to user
    btn = gtk_button_new_with_label("Play radio");
    btn1 = gtk_button_new_with_label("Pause");
    btn2=gtk_button_new_with_label("Change station");
    btn3=gtk_button_new_with_label("Stop radio");
    
    // Setting button's length and width
    gtk_widget_set_size_request(btn, 70, 30);
    gtk_widget_set_size_request(btn1, 70, 30);
    gtk_widget_set_size_request(btn2, 70, 30);
    gtk_widget_set_size_request(btn3, 70, 30);
    
    // gtk Box pack widgets (buttons) into a GtkBox from start to end
    gtk_box_pack_start(GTK_BOX(vbox), btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn3, TRUE, TRUE, 0);

    // Adding colour to the respective buttons
    gtk_widget_modify_bg (GTK_WIDGET(btn), GTK_STATE_NORMAL, &color);
    gtk_widget_modify_bg (GTK_WIDGET(btn1), GTK_STATE_NORMAL, &color1);
    gtk_widget_modify_bg (GTK_WIDGET(btn2), GTK_STATE_NORMAL, &color2);
    gtk_widget_modify_bg (GTK_WIDGET(btn3), GTK_STATE_NORMAL, &color3);    

     // Conecting buttons to their respective station listeners
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(clicked_button), argv);
    g_signal_connect(G_OBJECT(btn1), "clicked", G_CALLBACK(clicked_button1), NULL);
    g_signal_connect(G_OBJECT(btn2), "clicked", G_CALLBACK(clicked_button2), argv);
    g_signal_connect(G_OBJECT(btn3), "clicked", G_CALLBACK(clicked_button3), NULL);

    g_signal_connect(G_OBJECT(window), "destroy",G_CALLBACK(gtk_main_quit), G_OBJECT(window));
    
    gtk_widget_show_all(window);
    
    gtk_main();

    return 0;
}
