/* CSD 331 Computer Networks Lab, Fall 2019
 Server Code
 Team: Devshree Patel(075), Muskan Matwani(027), Ratnam Parikh(036), Yesha Shastri(035)
 */
//Commands for compilation and running
//gcc server.c
//./a.out 127.0.0.1
/* Including all the header files required for socket programming */
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/*Defining all the variables used recurrently in the program*/
#define MAX_BUFFER_SIZE 256
#define MULTI_SERVER_IP "239.192.1.10"
#define BUFFER_SIZE 4096
#define SERVER_PORT 5432
#define BUFFER_SIZE_SMALL 1024
#define NO_OF_STATIONS 3

//Defining structure for station info request
typedef struct station_info_request_t {
    uint8_t type;
} station_info_request;

//Init Function for station_info_request_t
station_info_request initStationInfoRequest(station_info_request *sir){
    sir->type = 1;
    return *sir;
}

//Defining structure for station info
typedef struct station_info_t {
    uint8_t station_number;
    uint8_t station_name_size;
    char station_name[MAX_BUFFER_SIZE];
    uint32_t multicast_address;
    uint16_t data_port;
    uint16_t info_port;
    uint32_t bit_rate;
} station_info;

//Defining structure for site info
typedef struct site_info_t {
    uint8_t type;// = 10;
    uint8_t site_name_size;
    char site_name[MAX_BUFFER_SIZE];
    uint8_t site_desc_size;
    char site_desc[MAX_BUFFER_SIZE];
    uint8_t station_count;
    station_info station_list[MAX_BUFFER_SIZE];
} site_info;

//Init Function for station_info
site_info initSiteInfo(site_info *si){
    si->type = 10;
    return *si;
}

//Defining structure for station not found
typedef struct station_not_found_t{
    uint8_t type;// = 11;
    uint8_t station_number;
} station_not_found;

//Init Function for station_not_found
station_not_found initStationNotFound(station_not_found *snf){
    snf->type = 11;
    return *snf;
}

//Defining structure for song info
typedef struct song_info_t {
    uint8_t type;// = 12;
    uint8_t song_name_size;
    char song_name[MAX_BUFFER_SIZE];
    uint16_t remaining_time_in_sec;
    uint8_t next_song_name_size;
    char next_song_name[MAX_BUFFER_SIZE];
} song_info;

//Init Function for song_info
song_info initSongInfo(song_info *si){
    si->type = 12;
    return *si;
}

//Defining structure for station id path
typedef struct station_id_path_t
{
    int id;
    char path[BUFFER_SIZE_SMALL];
    int port;
} station_id_path;

//Creating array of objects of structures
station_info stations[NO_OF_STATIONS];
station_id_path stationIDPaths[NO_OF_STATIONS];

//Declaring threads for stations
pthread_t stationThreads[NO_OF_STATIONS];

//Declaring sleep variable
long idealSleep;

//Declaring variables for storing command line arguments
int argC;
char **argV;
//Function for getting the bit rate
int getBitRate(char *fName){
    int pid = 0;
    char *cmd1 = "mediainfo "; //Fetching all the media properties
    char *redirect = " | grep 'Overall bit rate                         : ' > info.txt";//Writing down the output (bit rate) into a file
    char cmd[256];
    strcpy(cmd, cmd1);
    strcat(cmd, fName);
    strcat(cmd, redirect);
    system(cmd);
        // execl(params[0], params[1], params[2],params[3],NULL);
    wait(NULL);
    FILE* fp = fopen("info.txt", "r");
    char *str = "Overall bit rate                         : ";
    char buf[256];
    memset(buf, '\0', sizeof(buf));//setting buffer to 0
    char *s;
    fgets(buf, sizeof(buf), fp);
    char* p = strstr(buf,"Kbps\n");//string representing bit rate
    memset(p, '\0', strlen(p));//setting string to 0

    s = strstr(buf,str);
    s = s+strlen(str);
    
    char buf1[256];
    memset(buf1, '\0', 256);

    int i=0,j=0;
    for(i=0;i<strlen(s)+1;i++){
        if(s[i] == ' ')
            continue;
        buf1[j++] = s[i];
    }

    int br = atoi(buf1);//char array to int conversion
    return br;
}
//defining the function for calculation of bit rate
void BR_calc(char names[][BUFFER_SIZE_SMALL], int bitRate[], int sCount)
{
    for (int i = 0; i < sCount; i++)
    {
        bitRate[i] = getBitRate(names[i]);
    }
}

//Defining function for sending structures to stations
void *startStationListServer(void *arg)
{
    //Structure object initialised
    struct sockaddr_in sin;
    int len;
    int socket1, new_socket;
    char str[INET_ADDRSTRLEN];
    //Server IP initialised and declared
    char *serverIP;
    if (argC > 1)
        serverIP = argV[1];
    /* build address data structure */
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET; //assigning family
    sin.sin_addr.s_addr = inet_addr(serverIP);//assigning server address
    sin.sin_port = htons(SERVER_PORT);//assigning server port
    /* setup passive open */
    if ((socket1 = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed!!");//generating error on failure of socket
        exit(1);//program is terminated if socket is not created
    }
    
    inet_ntop(AF_INET, &(sin.sin_addr), str, INET_ADDRSTRLEN);//converting address from network format to presentation format
    printf("Server is using address %s and port %d.\n", str, SERVER_PORT);
    //Binding server to the address
    if ((bind(socket1, (struct sockaddr *)&sin, sizeof(sin))) < 0)
    {
        perror("Bind failed");//generating error if bind not done
        exit(1);//termination on failure of bind
    }
    else
        printf("Bind done successfully\n");
    
    listen(socket1, 5);//call for listen where pending request can be atmost 5
    //Sending structures infinitely to client
    while (1)
    {
        //Accepting the connection request from client
        if ((new_socket = accept(socket1, (struct sockaddr *)&sin, (unsigned int *)&len)) < 0)
        {
            perror("Accept failed");//Generating error if accept command fails
            exit(1);//terminating on failure of accept call
        }
        printf("Client and server connected! Starting to send structures\n");
        uint32_t nos = 3;//initialising no of stations
        nos = htonl(nos);//sending int requires this conversion
        send(new_socket, &nos, sizeof(uint32_t), 0);//no of stations(int) is sent
        
        for (int i = 0; i < NO_OF_STATIONS; i++)//sending the station info for all the stations specified
        {
            send(new_socket, &stations[i], sizeof(station_info), 0);
        }
    }
}
//Defining function fill stations
void StationDetails()
{
    station_info stat_info1;
    station_id_path stat_id_path;
    
    //Initialising station1 info into structure variables
    stat_info1.station_number = 1;
    stat_info1.station_name_size = htonl(strlen("Station 1"));
    strcpy(stat_info1.station_name, "Station 1");
    stat_info1.data_port = htons(8200);
    stat_id_path.port = 8200;
    stat_id_path.id = 1;
    strcpy(stat_id_path.path, "./Station_1/");
    
    //Copying station1 info and path
    memcpy(&stations[0], &stat_info1, sizeof(station_info));
    memcpy(&stationIDPaths[0], &stat_id_path, sizeof(station_id_path));
    
    //Clearing out station1 info and path
    bzero(&stat_info1, sizeof(station_info));
    bzero(&stat_id_path, sizeof(station_id_path));
    
    //Initialising station2 info into structure variables
    stat_info1.station_number = 2;
    stat_info1.station_name_size = htonl(strlen("Station 2"));
    strcpy(stat_info1.station_name, "Station 2");
    stat_info1.data_port = htons(8201);
    stat_id_path.port = 8201;
    stat_id_path.id = 2;
    strcpy(stat_id_path.path, "./Station_2/");
    
    //Copying station2 info and path
    memcpy(&stations[1], &stat_info1, sizeof(station_info));
    memcpy(&stationIDPaths[1], &stat_id_path, sizeof(station_id_path));
    
    //Clearing out station2 info and path
    bzero(&stat_info1, sizeof(station_info));
    bzero(&stat_id_path, sizeof(station_id_path));
    
    //Initialising station3 info into structure variables
    stat_info1.station_number = 3;
    stat_info1.station_name_size = htonl(strlen("Station 3"));
    strcpy(stat_info1.station_name, "Station 3");
    stat_info1.data_port = htons(8202);
    stat_id_path.port = 8202;
    stat_id_path.id = 3;
    strcpy(stat_id_path.path, "./Station_3/");
    
    //Copying station3 info and path
    memcpy(&stations[2], &stat_info1, sizeof(station_info));
    memcpy(&stationIDPaths[2], &stat_id_path, sizeof(station_id_path));
}
//Function for starting / setting up station
void *startStation(void *arg)
{
    //Parsing directory and opening songs
    station_id_path *stat_id_path = (station_id_path *)arg;
    DIR *dir;
    struct dirent *entry;
    int sCount = 0;
    //path for song file
    printf("Path:-- %s\n", stat_id_path->path);
    //searching in the directory and then reading all the songs
    if ((dir = opendir(stat_id_path->path)) != NULL)
    {
        //Reading all the files with extension mp3 and then collecting them
        while ((entry = readdir(dir)) != NULL)
        {
            if (strstr(entry->d_name, ".mp3") != NULL)
                ++sCount;//counter for songs with mp3 extension
        }
        closedir(dir);//closing directory
    }
    else
    {
        /* could not open file in the directory */
        perror("Could not find file in the directory");
        return 0;
    }
    
      
    char songs[sCount][BUFFER_SIZE_SMALL];    //Declaring 2D array for path of the songs and their size
    char sNames[sCount][BUFFER_SIZE_SMALL];  //Declaring 2D array for names of the songs and their size
    
    FILE *songFiles[sCount];
    int bitRates[sCount];
    
    for (int i = 0; i < sCount; i++)
    {
        memset(songs[i], '\0', BUFFER_SIZE_SMALL);
        strcpy(songs[i], stat_id_path->path);     //storing path of the songs
    }
    
    int cur = 0;
    if ((dir = opendir(stat_id_path->path)) != NULL)       //Checks for station directory is not empty
    {
        while ((entry = readdir(dir)) != NULL)       //proceeds to read the songs if directory is not null
        {
            if (strstr(entry->d_name, ".mp3") != NULL)
            {
                memcpy(&(songs[cur][strlen(stat_id_path->path)]), entry->d_name, strlen(entry->d_name) + 1);
                strcpy((sNames[cur]), entry->d_name);    //stores names of the available songs
                 
                songFiles[cur] = fopen(songs[cur], "rb");  //Opening the file
                if (songFiles[cur] == NULL)
                {
                    perror("No song file present in the directory");  //Displaying error for no song files
                    exit(1);
                }
                
                cur++;   //incrementing value to check for all the files in the directory
            }
        }
        closedir(dir);  //closing the current directory
    }
    
    //Creating Song_Info Structures
    song_info sInfo[sCount];
    
    for (int i = 0; i < sCount; i++)
        bzero(&sInfo[i], sizeof(song_info));   //zeros out the struct for information of songs
    
    for (int i = 0; i <= sCount; i++)
    {
        initSongInfo(&sInfo[i]);   //calling initSongInfo for the number of songs
        printf("song info : %hu p = %p\n", (unsigned short)sInfo[i].type, &sInfo[i].type);
    }
    
    for (int i = 0; i < sCount; i++)
    {
        //Fetching size and name of the song for the current song
        sInfo[i].song_name_size = (uint8_t)strlen(sNames[i]) + 1;
        printf("%d", sInfo[i].song_name_size);
        strcpy((sInfo[i].song_name), sNames[i]);
        
        //Fetching size and name of the next song
        sInfo[i].next_song_name_size = (uint8_t)strlen(sNames[(i + 1) % sCount]) + 1;
        strcpy((sInfo[i].next_song_name), sNames[(i + 1) % sCount]);
    }
    
    //Displaying information about songs
    for (int i = 0; i < sCount; i++)
    {
        printf("%s\n", songs[i]);
        printf("Song info : %hu p = %p\n", (unsigned short)sInfo[i].type, &sInfo[i].type);
    }
    
    int socket1;            //socket variable
    struct sockaddr_in sin; //object that refers to sockaddr structure
    int len;
    char buf[BUFFER_SIZE_SMALL];
    char *mcast_addr; /* multicast address */
    
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 20000000L;
    //Multicast address initialisation
    mcast_addr = "239.192.1.10";
    
    //socket creation for UDP multicase
    if ((socket1 = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed!!");//Error if socket is not created
        exit(1);
    }
    
    //build address data structure
    memset((char *)&sin, 0, sizeof(sin));//setting address variable to 0 using memset
    sin.sin_family = AF_INET;//assigning family
    sin.sin_addr.s_addr = inet_addr(mcast_addr);//assigning multicast address
    sin.sin_port = htons(stat_id_path->port);//assigning port number
    printf("\nStarting station ID : %d!\n\n", stat_id_path->id);
    
    /* Send multicast messages */
    memset(buf, 0, sizeof(buf));
    
    int curSong = -1;
    while (1)
    {
        //Choosing songs one by one
        curSong = (curSong + 1) % sCount;
        //Pointer for song
        FILE *song = songFiles[curSong];
        //In case when song is not found
        if (song == NULL)
            printf("Song not found!!\n");
        //Printing song number and song name
        printf("Curent Song number = %d current Song name= %p\n", curSong, song);
        rewind(song);//Setting pointer to the beginning of file
        
        int size = BUFFER_SIZE_SMALL;
        int counter = 0;
        //Printing structure which is sent
        printf("Sending Structure : current Song number= %d. Song_Info->type = %hu p = %p\n", curSong, (unsigned short)sInfo[curSong].type, &sInfo[curSong].type);
        
        if ((len = sendto(socket1, &sInfo[curSong], sizeof(song_info), 0, (struct sockaddr *)&sin, sizeof(sin))) == -1)
        {
            perror("Server sending failed");
            exit(1);
        }
        // calculating sleep time in accordance with bit rate
            float bitrate = bitRates[curSong];
            idealSleep = ((BUFFER_SIZE_SMALL * 8) / bitrate) * 500000000;
            
            // if sleep time is less than zero, assigning it to the default value specified
            if (idealSleep < 0)
                idealSleep = ts.tv_nsec;
            
            // if sleep time is greater than zero, assigning it to the idealSleep
            if (ts.tv_nsec > idealSleep)
                ts.tv_nsec = idealSleep;
            
            while (!(size < BUFFER_SIZE_SMALL))
            {
                // Sending the contents of song
                size = fread(buf, 1, sizeof(buf), song);
                
                if ((len = sendto(socket1, buf, size, 0, (struct sockaddr *)&sin, sizeof(sin))) == -1)
                {
                    perror("server: sendto");
                    exit(1);
                }
                if (len != size)
                {
                    printf("ERROR!!");
                    exit(0);
                }

                // Delaying the in between packet time in order to reduce packet loss at the client side
            // Delaying it with the time assigned to idealSleep
                nanosleep(&ts, NULL);
                memset(buf, 0, sizeof(buf));  //Setting buffer to 0
            }
        }
        //closing the socket
        close(socket1);
}


int main(int argc, char *argv[])
{
    //Assigning command line arguments to variables
    argC = argc;
    argV = argv;
    //Initializing Stations
    StationDetails();
    
    // Starting TCP Server
    //Declaring and creating pthread for starting TCP connection
    pthread_t tTCPid;
    pthread_create(&tTCPid, NULL, startStationListServer, NULL);
    //Starting all stations
    for (int i = 0; i < NO_OF_STATIONS; i++)
    {
        //creating thread for each station
        pthread_create(&stationThreads[i], NULL, startStation, &stationIDPaths[i]);
    }
    //waits for the thread tTCPid to terminate
    pthread_join(tTCPid, NULL);
    return 0;
}
