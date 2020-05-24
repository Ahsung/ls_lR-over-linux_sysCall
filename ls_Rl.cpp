#include<stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/sysmacros.h>
#include <pwd.h>
#include <grp.h>
#include<time.h>
#include<unistd.h>
#include<map>
#include<algorithm>
using namespace std;
#define str_MAX 1000     // filePath Name MAX Length 

char info[11];          // file permission bit info
int hardlink;           // hardlink cont
char uid[str_MAX];      
char gid[str_MAX];
int filesize;           
char lastModTime[str_MAX];  
char fileName[str_MAX];
int maj;                // device major Number
int mir;                // device minor Number

int totalBlock; // totalBlock cnt of current dir

//Dynamic printf format size
int hlsize;
int uidsize;
int gidsize;
int flsize;
int mjsize;
int misize;

map<long,bool> visitdir; 

//Print
void printDir(const char * dirpath,const char * nextdir,const char * restorePath = "..");
int update_gb_stat(const char * pathName);
char * timeformat(struct tm *modtm,char * t);
char itos_div10(int * d);
int getIntLen(int a);
int RenewStrLen(const char * pathName);
void show_gb_stat(int ftype);

//Oder
void quickSort(int l,int r,char **str_list);  // you can Use  sort(src,dst,lscmp) from algorithm header
int lscmp(const char * a, const char * b);
void swapStr(char ** fa,char ** fb);
int charType(char a);

//etc..
void freeStrList(char **ch,int size);
void reSetval();


int main(int argc, char **argv)
{
    struct stat buf;

    /*   "." start   */
    if(argc < 2){
        if(lstat(".",&buf) < 0)
                 fprintf(stderr,"read permission denied\n");   

        if(!visitdir.count(buf.st_ino))
                visitdir[buf.st_ino]=true;

        printDir(".","."); 
    }

    /*   argument  start  */
    else{
        int i;
        char cwd[str_MAX];
        getcwd(cwd,sizeof(cwd));

        //sort(argv+1,argv+argc,lscmp);
        quickSort(1,argc-1,argv);
        
        char **nomal_file = (char **)malloc(sizeof(char**)*argc); int nfsize = 0;
        char **dir_file = (char **)malloc(sizeof(char**)*argc); int dfsize = 0;
        int *dir_ino = (int *)malloc(sizeof(int *)*argc);

        //divide dir, nomal_file
        for(i=1;i < argc;i++){
            if(lstat(argv[i],&buf) < 0){
                 fprintf(stderr,"read permission denied : %s\n",argv[i]);
                 continue;
            }          

            // if dir
            if(S_ISDIR(buf.st_mode)) 
                 dir_ino[dfsize] = buf.st_ino, dir_file[dfsize++] = argv[i];
            // not
            else
                nomal_file[nfsize++] = argv[i],RenewStrLen(argv[i]);            
        }

        //nomal file print
        for(i=0;i < nfsize; i++)
            show_gb_stat(update_gb_stat(nomal_file[i]));

        if(nfsize && dfsize) printf("\n"); 

        //dir print
        for(i=0; i < dfsize; i++){
            visitdir.clear();

             //if not root, remove last charact '/'
            int len = strlen(dir_file[i]);
            if(len > 1 && dir_file[i][len-1] == '/')
                dir_file[i][len-1] ='\0';

            if(!visitdir.count(dir_ino[i]))
                    visitdir[dir_ino[i]]=true;
              
            printDir(dir_file[i],dir_file[i],cwd);
        }
    }
}



//-------------------------------------------------Function definition----------------------------------------------------

/*
 print file list in current dir
 recursive all dir
*/
void printDir(const char * dirpath,const char * nextdir,const char * restorePath){
    
    //fail change dir
    if( chdir(nextdir) == -1){
        fprintf(stderr,"cd permission denied: %s\n",dirpath);
        return;
    }

    //current all file list str
    char ** str_list = (char **)calloc(10,sizeof(char *));
    int csize = 0;
    int msize = 10;
    int i;
    
    //current dir list str
    char ** dir_list = (char **)calloc(10,sizeof(char *));
    int dcsize = 0;
    int dmsize = 10;

    struct dirent *item;
    DIR *cur;

   if(  (cur = opendir(".")) == NULL )
   {
       printf("ls: cannot open directory %s\n",dirpath);
       return ;
   }

    // printf format setting and get file list 
   for(;;)
    {
        item = readdir(cur);
        if (item == NULL)
            break;
        if(item->d_name[0] == '.') continue;
        
        //insert file name to list
        str_list[csize] = (char *)calloc(strlen(item->d_name)+1,sizeof(char));
        strcpy(str_list[csize++],item->d_name);
        if(csize == msize){
            msize *=2;
            str_list = (char **)realloc(str_list,(msize)*sizeof(char *));
        }

        RenewStrLen(item->d_name);
    }

    printf("%s:\n",dirpath);
    printf("total %d\n",totalBlock/2);

    quickSort(0,csize-1,str_list); // Sort file list

    // Print!
    for(i = 0; i < csize; i++){
        int ft = update_gb_stat(str_list[i]);
        show_gb_stat(ft);
        
        // insert dir to list
        if(ft == 1 ){
            dir_list[dcsize] = (char *)calloc(strlen(fileName)+1,sizeof(char));
            strcpy(dir_list[dcsize++],fileName);
            if(dcsize == dmsize){
                dmsize *=2;
                dir_list = (char **)realloc(dir_list,(dmsize)*sizeof(char *));
            }
        }

    }

    //recursive dir
    for(i = 0; i < dcsize; i++){
        char nextdirpath[str_MAX];
        strcpy(nextdirpath,dirpath);
        strcat(nextdirpath,"/");
        strcat(nextdirpath,dir_list[i]);
        printf("\n");
        reSetval();
        printDir(nextdirpath,dir_list[i]);
    }

    chdir(restorePath); // restore path
    freeStrList(dir_list,dcsize);
    freeStrList(str_list,csize);
    closedir(cur);
}

void reSetval(){
    totalBlock = 0;
    hlsize = 0;
    uidsize = 0;
    gidsize= 0;
    flsize = 0;
    mjsize= 0;
    misize= 0;
} 

/*
set file info! (global var)
permission bit
uid, gid
fileSize or device number
last modified Time
file name and link path
*/
int update_gb_stat(const char * pathName){
    int retFtype = 0;

    struct stat buf;
    char ftype[] = "----------";
    strcpy(fileName,pathName);

    if(lstat(pathName,&buf) < 0) return -1;
    
    //permission bit
    if(S_ISREG(buf.st_mode)) ftype[0] = '-';
    if(S_ISDIR(buf.st_mode)) {
        ftype[0] = 'd',retFtype = 1;
        if(!visitdir.count(buf.st_ino))
            visitdir[buf.st_ino]=true;
        else
            retFtype = 0;
    }
    if(S_ISCHR(buf.st_mode)) ftype[0] = 'c',retFtype = 2;
    if(S_ISBLK(buf.st_mode)) ftype[0] = 'b',retFtype = 2;
    if(S_ISFIFO(buf.st_mode)) ftype[0] = 'p';
    // link file.
    if(S_ISLNK(buf.st_mode)){
     ftype[0] = 'l';
     char linkbuff[str_MAX] = "";
     readlink(pathName,linkbuff,str_MAX);
     strcpy(fileName,pathName);
     char le[] = " -> ";
     strcat(fileName,le);
     strcat(fileName,linkbuff);
    }
    
    // device Number global var
    if(ftype[0] == 'b'|| ftype[0] == 'c'){
         maj = major(buf.st_rdev);
         mir = minor(buf.st_rdev);     
     }
        
    filesize = buf.st_size; // file size global var


    if(buf.st_mode & S_IRUSR) ftype[1] = 'r';
    if(buf.st_mode & S_IWUSR) ftype[2] = 'w';
    if(buf.st_mode & S_IXUSR) ftype[3] = 'x';

    if(buf.st_mode & S_IRGRP) ftype[4] = 'r';
    if(buf.st_mode & S_IWGRP) ftype[5] = 'w';
    if(buf.st_mode & S_IXGRP) ftype[6] = 'x';

    if(buf.st_mode & S_IROTH) ftype[7] = 'r';
    if(buf.st_mode & S_IWOTH) ftype[8] = 'w';
    if(buf.st_mode & S_IXOTH) ftype[9] = 'x';

    if(buf.st_mode & S_ISUID)
    { 
        if(ftype[3] == 'x')
            ftype[3] = 's';
        else
            ftype[3] = 'S';
    }

    if(buf.st_mode & S_ISGID) 
    {
        if(ftype[6] == 'x')
            ftype[6] = 's';
        else
            ftype[6] = 'S';
    }

    if(buf.st_mode & __S_ISVTX) 
    {
        if(ftype[9] == 'x')
            ftype[9] = 't';
        else
            ftype[9] = 'T';
    }

    strcpy(info,ftype); //permission bit global var
    
    hardlink = buf.st_nlink; //link cnt global var


    // if not exist Name, replace (u|g)id
    if(getpwuid(buf.st_uid) == NULL){
        int utmp = buf.st_uid;
        int lsize = getIntLen(utmp);
        int i;
        for(i = lsize-1; i >= 0; i--)
            uid[i] = itos_div10(&utmp);
        uid[lsize] = '\0';
    }
    else
        strcpy(uid,getpwuid(buf.st_uid)->pw_name); //uid global var

    if(getgrgid(buf.st_gid) == NULL){
        int utmp = buf.st_gid;
        int lsize = getIntLen(utmp);
        int i;
        for(i = lsize-1; i >= 0; i--)
            gid[i] = itos_div10(&utmp);
            gid[lsize] = '\0';
    }
    else
        strcpy(gid,getgrgid(buf.st_gid)->gr_name); //gid global var

    
    
    struct tm *modtm = localtime(&buf.st_mtime);
    timeformat(modtm,lastModTime);  //modified global var

    return retFtype; // file type flag or error
}

void show_gb_stat(int ft){
    if(ft == -1){
            printf("ls: cannot read file %s\n",fileName);
        }
        else if(ft == 2)
            printf("%s %*d %-*s %-*s %*d, %*d %s %s\n",
            info, hlsize,hardlink,uidsize,uid,gidsize,gid,mjsize,maj
            ,misize,mir,lastModTime,fileName);
        else
            printf("%s %*d %-*s %-*s %*d %s %s\n",
            info, hlsize,hardlink,uidsize,uid,gidsize,gid,flsize,filesize
            ,lastModTime,fileName);
};

int getIntLen(int a){
    int n = 0;
    while(a > 0){
        n++;
        a /=10;
    }
    return n;
}

/*
set printf Format var
*/
int RenewStrLen(const char * pathName){
    struct stat buf;
    if(lstat(pathName,&buf) < 0) return -1;

    //hardlink length
    int chlsize = getIntLen(buf.st_nlink);
    if(chlsize > hlsize) hlsize = chlsize;

    //uid, gid length
    if(getpwuid(buf.st_uid) != NULL ){
        int cuidsize = strlen(getpwuid(buf.st_uid)->pw_name);
        if ( cuidsize > uidsize) uidsize = cuidsize;
    }
    else{
        int cuidsize = getIntLen(buf.st_uid);
        if ( cuidsize > uidsize) uidsize = cuidsize;
    }

    if(getgrgid(buf.st_gid) != NULL ){
        int cgidsize = strlen(getgrgid(buf.st_gid)->gr_name);
        if ( cgidsize > gidsize) gidsize = cgidsize;
    }
    else{
        int cgidsize = getIntLen(buf.st_gid);
         if ( cgidsize > gidsize) gidsize = cgidsize;
    }
     //device num or filesize length
    if(S_ISCHR(buf.st_mode) || S_ISBLK(buf.st_mode)){
        int mj = getIntLen(major(buf.st_rdev));
        if(mj > mjsize) mjsize = mj;
        int mi = getIntLen(minor(buf.st_rdev));
        if(mi > misize) misize = mi;

        if(mj+mi+2 > flsize) flsize = mj+mi+2;
    } 
    else{
        int cflsize = getIntLen(buf.st_size);
        if(cflsize > flsize) flsize = cflsize;
    }

    //count totalBlock
    totalBlock += buf.st_blocks;
    
    return 0;
}

//remoce list
void freeStrList(char **ch,int size){
    int i;
    for(i = 0; i < size; i++){
        free(ch[i]);
    }
    free(ch);
};

void swapStr(char ** fa,char ** fb){
    char * tmp = *fa;
    *fa = *fb;
    *fb = tmp;
}

// for Sorting
int charType(char a){
    if(a == '\0') return -1;
    if(a <= '9' && a>= '0') return 0;
    if(a <= 'z' && a>= 'a') return 1;
    if(a <= 'Z' && a>= 'A') return 2;
    else return 3;
}

// sort file list 
void quickSort(int l,int r,char **str_list){  
    int pivot;
    if(r > l){
        int i,j;
        pivot = l;
        for(i = l+1; i <= r; i++){
            if(lscmp(str_list[l],str_list[i]) == 1 ){
                pivot++;
                swapStr(&str_list[i],&str_list[pivot]);
            }
        }

        swapStr(&str_list[l],&str_list[pivot]);
        
        quickSort(l,pivot-1,str_list);
        quickSort(pivot+1,r,str_list);
    }
}

/* locale us.utf-8 compare Priority */
int lscmp(const char * a, const char * b){
    int i=0;
    int j=0;
    int d = 'a' - 'A';
    int winflag = -1;

    while(a[i] != '\0' && b[j] != '\0'){
        while(charType(a[i]) == 3) i++;
        while(charType(b[j])== 3) j++;

        if(a[i] == '\0' || b[j] == '\0') break;
        
        char at = a[i],bt = b[j];
        int aty = charType(at);
        int bty = charType(bt);
        if(aty == 2) at +=d;
        if(bty == 2) bt +=d;
        
        if(at < bt) return 0;
        if((winflag ==-1)&&(at == bt) && ((aty == 2) || (bty == 2)) && (aty != bty)){
                if(aty == 2) winflag = 1;
                else if(bty == 2) winflag = 0;
        }

        if(at > bt) return 1;
        i++; j++;
        while(charType(a[i]) == 3) i++;
        while(charType(b[j])== 3) j++;
    }
    if(a[i] == '\0' && b[j] == '\0'){
        if(winflag != -1) return winflag;
        else 
            return strlen(a)  > strlen(b);
    }
    if(a[i] == '\0') return 0;
    if(b[j] == '\0') return 1;
}

/* return string time format */
char * timeformat(struct tm *modtm,char * t){
    char tmp[] = "0000-00-00 00:00";
    if(modtm == NULL)
    {
        strcpy(t,tmp);
        return t;
    }    

    int year = modtm->tm_year + 1900;
    int mon = modtm->tm_mon + 1;
    int day = modtm->tm_mday;
    int hour = modtm->tm_hour;
    int min = modtm->tm_min;

    int i;
    //convert time format
    for(i=3; i >=0; i--) tmp[i] = itos_div10(&year);
    for(i=6; i >=5; i--) tmp[i] = itos_div10(&mon);
    for(i=9; i >=8; i--) tmp[i] = itos_div10(&day);
    for(i=12; i >=11; i--) tmp[i] = itos_div10(&hour);
    for(i=15; i >=14; i--) tmp[i] = itos_div10(&min);

    strcpy(t,tmp);
    return t;
}

char itos_div10(int * d){
    char res = *d%10 + '0';
    *d = *d/10;
    return res;
}
