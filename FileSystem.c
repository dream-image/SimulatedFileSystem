#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>


#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

// FAT16
#define BLOCKSIZE 1024
#define SIZE 1024 * 1000
#define END 65535
#define FREE 0
#define ROOTBLOCKNUM 2
#define MAXOPENFILE 10
#define ITSELF "."
#define PARENT ".."
#define DIR 0
#define NORMALFILE 1
#define FILENAME "/root/fileSystem/myVfile.VF"
/*
    说明：我基本采用全部遍历的方式，以降低编写的时候人为计算错误率

 */

typedef struct FCB
{
    char filename[8];        // 文件名
    char exname[4];          // 拓展名
    unsigned char attribute; // 属性 0为目录，1为普通文件
    char time[9];            // 创建时间
    char date[11];           // 创建日期
    unsigned short first;    // 起始盘块号
    unsigned long length;    // 文件长度(字节数)
    char free;               // 目录项：0为空，1为已分配

} fcb;

typedef struct FAT
{
    unsigned short id;
} fat;

typedef struct USEROPEN
{
    char filename[8];
    char exname[4];
    char time[9];         // 创建时间
    char date[11];        // 创建日期
    unsigned short first; // 起始盘块号
    unsigned long length; // 文件长度
    char dir[80];         // 文件路径
    int count;            // 读写指针的位置
    char fcbstate;        // 文件的FCB是否被修改，是则为1，否为0
    int topenfile;        // 打开表项是否为空。0为空，否为已占用
    int fd;               // 文件描述符
    struct USEROPEN *next;
    struct USEROPEN *header; // 永远指向头部
} useropen;
typedef struct BLOCK0
{
    char infomation[200];      // 存储描述信息，磁盘块大小，磁盘块数量等
    unsigned short root;       // 根目录的起始盘块号
    unsigned char *startblock; // 虚拟磁盘上数据区开始的位置
} block0;
unsigned char *myvhard; // 指向虚拟磁盘的起始地址
// unsigned openfilelist[MAXOPENFILE]; // 打开表文件数组
int curdir;                  // 当前文件描述符
char currentdir[80] = "";    // 当前目录
unsigned char *startp;       // 数据区开始的位置
unsigned char *currentBlock; // 当前处于的盘块地址
unsigned char *startFAT;     // FAT表开始地址
useropen *openFileList;      // 打开的文件单项链表 采用先入先出

int split(char *pathname, char result[][8], char delimiter);                     // 根据符号切割字符串
int spliteDot(char *filename, char result[2][8]);                                // 按点切割获取文件名和后缀名
int splitSpaceKey(char *str, char result[3][80]);                                // 按空格切割
int isCurrentDir(char *str);                                                     // 路径是否是当前路径下
int getFreeBlock(struct FCB *F);                                                 // 获取到空闲的盘块的块号
int my_getline(char *line, int max_size);                                        // 读取一行输入
void getFileInfo(struct FCB *F, char *name, char *type, char *space);            // 文件信息拼接
void initFCB(struct FCB *F, int first, struct FCB *parent);                      // 初始化 . 和 .. FCB
void setFileTime(struct FCB *F, struct FCB *obj);                                // 初始化FCB时间或者是复制FCB时间
void recombination(char *pathname, char result[][8], int length, char *newPath); // 从文件路径中解析出路径
void initNormalFCB(struct FCB *F, int first, char *name, char *exname);          // 初始化普通文件的FCB
void initUserOpenList();


void startsys();
void my_format();
void ls();
int cd(char *str, int mode);
int mkdir(char *str);
int rmdir(char *str, int mode);
void rmAll(struct FCB *F);
int openFile(char *str);
int createFile(char *str);
int removeFile(char *str);
int writeFile(char *str, int mode);
int readFile(char *str, int len);
// 返回值是切片个数 如果返回值是0，则出错
int split(char *str, char result[][8], char delimiter)
{
    if (strlen(str) == 0)
    {
        return 0;
    }
    char *filename = NULL, *ptr = NULL;
    filename = strtok(str, "/");
    if (strlen(filename) > 7)
    {
        return 0;
    }
    int length = 0;
    while (filename != NULL)
    {
        strcpy(result[length], filename);
        length++;
        if (strlen(filename) > 7)
        {
            return 0;
        }
        filename = strtok(NULL, "/");
    }
    return length;
}

void recombination(char *pathname, char result[][8], int length, char *newPath)
{ // 从文件路径中解析出路径
    char a[80] = "";
    if (pathname[0] == '/')
    {
        for (int i = 0; i < length - 1; i++)
        {
            strcat(a, "/");
            strcat(a, result[i]);
        }
        strcpy(newPath, a);
        return;
    }
    // 跑到这里说明不是以/开头的
    strcat(a, result[0]);
    for (int i = 1; i < length - 1; i++)
    {
        strcat(a, "/");
        strcat(a, result[i]);
    }
    strcpy(newPath, a);
}
void getFileInfo(struct FCB *F, char *name, char *type, char *space)
{
    char unit[8] = "";
    float length = F->length;
    if (length > 1024)
    {
        length = length / 1024;
        strcat(unit, "KB");
    }
    else
    {
        strcat(unit, "B");
    }

    sprintf(space, "%.2lf", length);
    strcat(space, unit);
    // printf("%s\n",space);
    if (F->attribute == 0)
    {
        strcat(type, "DIR");
    }
    else
    {
        strcat(type, "FILE");
    }

    if (strlen(F->exname) != 0)
    {
        strcat(name, F->filename);
        strcat(name, ".");
        strcat(name, F->exname);
    }
    else
    {
        strcat(name, F->filename);
    }
    // printf("dasdas");
    // printf("%s    <%s>     %s\n", name, type, space);
}

int getFreeBlock(struct FCB *F)
{
    if (F != NULL)
    {
        fat *p = (fat *)startFAT;
        // printf("myhard2:%d\n",myvhard);
        // printf("start:%d\n",p);
        for (int i = 0; i < 2 * BLOCKSIZE / sizeof(fat); i++)
        {
            // printf("@@%d\n",i);
            if (p->id == FREE)
            {
                // printf("free:%d\n",p);
                F->first = i;
                p->id = END;
                return 1;
            }
            p++;
        }
        // 如果走到这，说明没空间分配了
        return 0;
    }
    else
    {
        fat *p = (fat *)startFAT;
        for (int i = 0; i < 2 * BLOCKSIZE / sizeof(fat); i++)
        {
            if (p->id == FREE)
            {
                // printf("\n$$%d$$\n",i);
                return i;
            }
            p++;
        }
        return 0;
    }
}

int isCurrentDir(char *str)
{
    if (strlen(str) == 0)
    {
        return 1;
    }
    char result[10][8];
    int num = split(str, result, '/');
    if (num == 0)
    {
        printf("文件路径有误");
        return 0;
    }
    if (num > 2)
    {
        printf("只允许对当前目录下子文件操作");
        return 0;
    }
    if (num == 2 && (str[0] != '.' || str[1] != '/'))
    {
        printf("只允许对当前目录下子文件操作");
        return 0;
    }
    if (num == 1 && str[0] == '/')
    {
        printf("只允许对当前目录下子文件操作");
        return 0;
    }
    return 1;
}

void initFCB(struct FCB *F, int first, struct FCB *parent)
{ // 如果parent给了，那就是设置父元素的FCB
    // 这里的parent我认为是当前所在目录的那个 . 的first
    F->free = 1;
    F->length = 0;
    F->attribute = DIR;
    memset(F->exname, 0, sizeof(F->exname));
    strcat(F->exname, "");
    if (parent == NULL)
    {
        strcat(F->filename, ITSELF);
        F->first = first;
        setFileTime(F, NULL);
        return;
    }
    strcat(F->filename, PARENT);
    F->first = parent->first;
    setFileTime(F, parent);
}

void setFileTime(struct FCB *F, struct FCB *obj)
{ // 第二个参数如果不是null就复制第二个参数的时间
    memset(F->date, 0, sizeof(F->date));
    memset(F->time, 0, sizeof(F->time));
    if (obj != NULL)
    {
        strcpy(F->date, obj->date);
        strcpy(F->time, obj->time);
        return;
    }
    time_t *now;
    now = (time_t *)malloc(sizeof(time_t));
    time(now);
    struct tm *nowtime = localtime(now);
    char hour[3] = "";
    sprintf(hour, "%d", nowtime->tm_hour);
    if (nowtime->tm_hour < 10)
    {
        strcat(F->time, "0");
    }
    strcat(F->time, hour);
    strcat(F->time, ":");
    char min[3] = "";
    sprintf(min, "%d", nowtime->tm_min);
    if (nowtime->tm_min < 10)
    {
        strcat(F->time, "0");
    }
    strcat(F->time, min);
    strcat(F->time, ":");
    char sec[3] = "";
    sprintf(sec, "%d", nowtime->tm_sec);
    if (nowtime->tm_sec < 10)
    {
        strcat(F->time, "0");
    }
    strcat(F->time, sec);
    char year[5] = "";
    sprintf(year, "%lu", nowtime->tm_year + 1900);
    strcat(F->date, year);
    strcat(F->date, "-");
    char month[3] = "";
    sprintf(month, "%d", nowtime->tm_mon + 1);
    if (nowtime->tm_mon + 1 < 10)
    {
        strcat(F->date, "0");
    }
    strcat(F->date, month);
    strcat(F->date, "-");
    char day[3] = "";
    sprintf(day, "%d", nowtime->tm_mday);
    if (nowtime->tm_mday < 10)
    {
        strcat(F->date, "0");
    }
    strcat(F->date, day);
    free(now);
}

void setUserOpen(struct FCB *F, struct USEROPEN *open)
{ // 将fcb的内容复制给useropen

    memset(open->filename, 0, sizeof(open->filename));
    memset(open->exname, 0, sizeof(open->exname));
    memset(open->time, 0, sizeof(open->time));
    memset(open->date, 0, sizeof(open->date));
    strcpy(open->filename, F->filename);
    strcpy(open->exname, F->exname);
    strcpy(open->time, F->time);
    strcpy(open->date, F->date);
    open->first = F->first;
    open->length = F->length;
    open->count = 0;
    open->fcbstate = 0;
    open->topenfile = 1;
}

void initNormalFCB(struct FCB *F, int first, char *name, char *exname)
{
    F->free = 1;
    F->length = 0;
    F->attribute = NORMALFILE;
    // printf("%s %s",name,exname);
    memset(F->exname, 0, sizeof(F->exname));
    strcpy(F->exname, exname);
    memset(F->filename, 0, sizeof(F->filename));
    strcpy(F->filename, name);
    F->first = first;
    fat *f = (fat *)startFAT;
    f += first;
    f->id = END;
    setFileTime(F, NULL);
}

int spliteDot(char *filename, char result[2][8])
{
    // int index=0;
    // // printf("%d\n",strlen(filename));
    // while(filename[index]!='.' && filename[index]!='\0'){
    //     index++;
    // }
    // // printf("%d\n",index);
    // if(index==strlen(filename)){
    //     return -1;
    // }
    int i = 0;
    for (; i < strlen(filename); i++)
    {
        if (filename[i] == '.')
        {
            break;
        }
        result[0][i] = filename[i];
    }
    result[0][i] = '\0';
    i++;
    int j = 0;
    for (; i < strlen(filename); i++, j++)
    {
        // printf("%c ",filename[i]);
        result[1][j] = filename[i];
    }
    result[1][j] = '\0';
    if (strlen(result[1]) > 3)
    {
        return 0;
    }
    if (strlen(result[1]) == 0)
    {
        return 1;
    }
    return 2;
}

int splitSpaceKey(char *str, char result[3][80]) // 按空格切割
{
    char *filename = NULL, *ptr = NULL;
    filename = strtok(str, " ");
    int length = 0;
    while (filename != NULL)
    {
        strcpy(result[length], filename);
        length++;
        if (length > 3)
        {
            return 0;
        }
        filename = strtok(NULL, " ");
    }
    return length;
}

int my_getline(char *line, int max_size)
{
    int c;
    int len = 0;
    while ((c = getchar()) != EOF && len < max_size)
    {
        line[len++] = c;
        if ('\n' == c)
            break;
    }

    line[len - 1] = '\0';
    return len;
}

void initUserOpenList(){
    // 初始化链表
    openFileList = (useropen *)malloc(sizeof(useropen));
    openFileList->topenfile = 0;
    openFileList->header = openFileList;
    useropen *header = openFileList, *s = openFileList;
    header->fd = 0;
    int x = 1;
    for (int i = 1; i < MAXOPENFILE; i++)
    {
        useropen *p = (useropen *)malloc(sizeof(useropen));
        p->header = header;
        p->topenfile = 0;
        p->fd = x;
        x++;
        s->next = p;
        s = p;
    }
    s->next = NULL;
}

// 上面都是辅助函数，下面是操作函数
void startsys()
{
    myvhard = (unsigned char *)malloc(SIZE);
    memset(myvhard, 0, SIZE);
    FILE *fp;
    if ((fp = fopen(FILENAME, "r")) != NULL)
    {
        fread(myvhard, SIZE, 1, fp);
        fclose(fp);
    }
    else
    {
        printf("未发现保存的数据，将重新格式化\n");
        // 将磁盘进行格式化
        my_format();
        return;
    }
    startFAT = myvhard + BLOCKSIZE;
    startp = myvhard + (1 + 2 * ROOTBLOCKNUM) * BLOCKSIZE;
    currentBlock = startp;
    initUserOpenList();
    strcpy(currentdir,"/root");
    printf(YELLOW"文件系统初始化完毕\n"NONE);
}

void my_format()
{

    // printf("myvhard:%d\n",myvhard);
    unsigned char *p = myvhard;
    block0 *b0 = (block0 *)(p);
    strcat(b0->infomation, "磁盘块共1000个，每个1024个字节");
    p += BLOCKSIZE; // 跑到fat表初始地址
    // printf("p:%d\n",p);
    fat *fat1 = (fat *)(p);
    fat *fat2 = (fat *)(p + ROOTBLOCKNUM * BLOCKSIZE);

    fat1->id = 1;
    // printf("fat1:%d\n",fat1);
    fat2->id = 1;
    fat1++;
    fat2++;
    fat1->id = END;
    fat2->id = END;
    fat1++;
    fat2++;
    for (int i = ROOTBLOCKNUM; i < 2 * BLOCKSIZE / sizeof(fat) - 1; i++)
    {
        fat1->id = FREE;
        fat2->id = FREE;
        fat1++;
        fat2++;
    }
    fat1->id = END;
    fat2->id = END;
    fcb *root = (fcb *)(p + 4 * BLOCKSIZE);
    initFCB(root, 0, NULL);
    fcb *q = root;
    q++;
    for (int i = 1; i < 2 * BLOCKSIZE / sizeof(fcb); i++)
    {
        q->free = FREE;
        q++;
    }
    b0->startblock = (p + 4 * BLOCKSIZE);
    startp = (p + 4 * BLOCKSIZE);
    currentBlock = (p + 4 * BLOCKSIZE);
    startFAT = p;
    strcat(currentdir, "/root");

    initUserOpenList();

    // printf("%d\n",currentBlock);
    printf("bi~bi~bi~bi~······初始化完毕······\n");
    // printf("/root>");
};

// 允许跨越展示
void ls(char *str)
{

    char result[10][8];
    int length;
    length = split(str, result, '/');
    if (length == 0 && strlen(str) != 0 && str[0] != '.')
    {
        printf("路径不存在");
        return;
    }
    if (strlen(str) != 0)
    {
        // printf("%d,%d",length,strlen(str) );
        // 说明现在不是直接展示当前目录的文件
        // 思路是跑到目标目录下，然后输出，然后再跑回来
        char saveDir[80] = "";
        strcpy(saveDir, currentdir);
        // printf("@%s\n",saveDir);
        // printf("???%s\n",currentdir);
        int flag = cd(str, 0);
        if (flag == 0)
        {
            printf("请输入正确目录\n");
            // printf("%s>",currentdir);
            return;
        }
        fcb *p = (fcb *)currentBlock;
        printf("目录       文件类型        文件大小      创建时间      创建日期\n");
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
        {
            if (p->free == 1)
            {
                char name[8] = "", type[5] = "", space[12] = "";
                getFileInfo(p, name, type, space);
                if(p->attribute==1){
                    printf("%-8s    <%s>          %-12s%s    %s\n", name, type, space, p->time, p->date);
                }
                else printf("%-8s    <%s>           %-12s%s    %s\n", name, type, space, p->time, p->date);
            }
            p++;
        }
        // printf("save:%s\n", saveDir);
        if (cd(saveDir, 0) == 0)
        {
            printf("出错了\n");
        }
        // printf("@@%s\n",currentdir);
        return;
    }
    if (strlen(str) == 0)
    {
        // printf("走到这了吗?");
        // 说明要展示当前目录下的文件

        fcb *p = (fcb *)(char *)currentBlock;
        printf("目录       文件类型        文件大小      创建时间      创建日期\n");
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
        {
            // printf("%d\n",p);
            if (p->free == 1)
            {
                char name[8] = "", type[5] = "", space[12] = "";
                getFileInfo(p, name, type, space);
                // printf("%d\n",i);
                if(p->attribute==1){
                    printf("%-8s    <%s>          %-12s%s    %s\n", name, type, space, p->time, p->date);
                }
                else printf("%-8s    <%s>           %-12s%s    %s\n", name, type, space, p->time, p->date);
            }
            p++;
        }
    }
}
// 允许跨越当前目录rmdir
int cd(char *str, int mode) // mode=1说明开启路径打印，mode=0说明不开启路径打印   返回值为0说明错误
{
    if (strlen(str) == 0)
    {
        return 1;
    }
    char backupDir[80] = "";
    strcpy(backupDir, currentdir);
    // printf("@@%s\n",currentdir);
    unsigned char *backupBlock = currentBlock; // 备份一下当前的状态
    char result[10][8];
    // printf("xxx%s\n",str);
    int length = split(str, result, '/');
    // printf("%s %s\n",result[1],result[2]);
    unsigned char *q = startp; // 拿到数据区开始地址，也就是root的开始地址

    // 没有 /  或者 有/ 但不是以/开头的
    if ((length == 1 || length > 1) && str[0] != '/')
    {
        fcb *p = (fcb *)currentBlock;
        int flag = 0;
        for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
        { // 如果循环内找到需要的fcb，那么会return出去
            if (p->free == 0)
                continue;
            if (strcmp(p->filename, result[0]) == 0 && p->attribute == 0)
            {
                // printf("%s\n",result[0]);
                q = BLOCKSIZE * (p->first) + startp; // 找到目标盘块起始地址
                currentBlock = q;                    // 将当前盘块地址切换为目标盘块起始地址
                if (strcmp(result[0], "..") == 0)
                {
                    char a[10][8];
                    int b = split(backupDir, a, '/');
                    memset(currentdir, 0, 80);
                    for (int x = 0; x < b - 1; x++)
                    {
                        strcat(currentdir, "/");
                        strcat(currentdir, a[x]);
                    }
                    p = (fcb *)currentBlock;
                }
                else if (strcmp(result[0], ".") != 0)
                {
                    strcat(currentdir, "/");
                    strcat(currentdir, result[0]); // 路径更新
                }

                if (length == 1 && mode == 1)
                {
                    // printf("%s>", currentdir);
                    return 1;
                }

                flag = 1;
                break;
            }
            p++;
        }
        if (flag == 0)
        {
            // 说明没找到
            currentBlock = backupBlock;
            memset(currentdir, 0, sizeof(currentdir));
            strcat(currentdir, backupDir);
            return 0;
        }
        // 如果跑到这 length至少为2
        for (int i = 1; i < length; i++)
        {
            // printf("%s\n",result[1]);
            flag = 0;
            for (int j = 0; j < BLOCKSIZE / sizeof(fcb); j++)
            {
                if (p->free == 0)
                    continue;
                if (strcmp(p->filename, result[i]) == 0 && p->attribute == 0)
                {
                    q = BLOCKSIZE * (p->first) + startp;
                    currentBlock = q;
                    strcat(currentdir, "/");
                    strcat(currentdir, result[i]);
                    flag = 1;
                    break;
                }
                p++;
            }
            // 如果跑到这里flag还为0，说明上面没找到
            if (flag == 0)
            {
                currentBlock = backupBlock;
                memset(currentdir, 0, sizeof(currentdir));
                strcat(currentdir, backupDir);
                return 0;
            }
        }
        // 跑到这说明都找到了
        if (mode == 1)
        {
            // printf("%s>", currentdir);
        }
        return 1;
    }
    // 如果执行到这，说明给的str里面以 / 开头 ，就说明路径可能需要多次跳转
    if (str[0] == '/' && strcmp(result[0], "root") == 0)
    {

        // 以 / 开头 并且第一个切出来root 说明要根据根目录跳转

        memset(currentdir, 0, sizeof(currentdir));
        strcat(currentdir, "/root");
        currentBlock = q;
        if (length == 1)
        { // 说明只有一个/root 或/root/
            currentBlock = q;
            if (mode == 1)
            {
                // printf("/root>");
            }
            return 1;
        }
        // 跑到这里说明不止有/root
        fcb *p = (fcb *)q;
        for (int i = 0; i < 2 * BLOCKSIZE / sizeof(fcb); i++)
        {
            if (p->free == 0)
                continue;
            if (strcmp(p->filename, result[1]) == 0 && p->attribute == 0)
            {
                q = BLOCKSIZE * (p->first) + startp; // 找到目标盘块起始地址
                currentBlock = q;                    // 将当前盘块地址切换为目标盘块起始地址
                strcat(currentdir, "/");
                strcat(currentdir, result[1]);
                if (length == 2)
                { // 长度等于2 说明只有一个 / 只需要在root查找一次就可以了
                    // strcat(currentdir, "/");
                    // strcat(currentdir, result[1]);
                    // printf("%s>", currentdir);
                    return 1;
                }
                // 如果跑到这，说明长度不止为2，那么需要循环了
                fcb *x = (fcb *)currentBlock;
                for (int j = 2; j < length; j++)
                {
                    int flag = 0;
                    for (int k = 0; k < BLOCKSIZE / sizeof(fcb); k++)
                    {
                        if (x->free == 0)
                            continue;
                        if (strcmp(x->filename, result[j]) == 0 && x->attribute == 0)
                        {
                            q = BLOCKSIZE * (x->first) + startp;
                            currentBlock = q;
                            strcat(currentdir, "/");
                            strcat(currentdir, result[j]);
                            flag = 1;
                            break;
                        }
                        x++;
                    }
                    if (flag == 0)
                    { // 说明有没找到的
                        currentBlock = backupBlock;
                        memset(currentdir, 0, sizeof(currentdir));
                        strcat(currentdir, backupDir);
                        return 0;
                    }
                }
                // 如果执行到这，说明都找到了
                if (mode == 1)
                {
                    // printf("%s>", currentdir);
                }
                return 1;
            }
            p++;
        }
        // 如果跑到这，说明没找到
        memset(currentdir, 0, sizeof(currentdir));
        strcat(currentdir, backupDir);
        currentBlock = backupBlock;
        return 0;
    }

    if (str[0] == '/' && strcmp(result[0], "root") != 0)
    {
        // 说明以/开头但是后面跟的不是root 直接错误
        return 0;
    }
    return 0;
}

// 仅支持当前目录下创建
int mkdir(char *str)
{

    char result[10][8];
    if (isCurrentDir(str) == 0)
    {
        return 0;
    }

    if (spliteDot(str, result) != 1)
    {
        printf("文件夹名称不允许包含“.” \n");
        return 0;
    }
    fcb *p = (fcb *)(char *)currentBlock;
    fcb *parent = p;
    fcb *x = p;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (x->attribute == 0 && strcmp(x->filename, str) == 0)
        {
            printf("文件已存在\n");
            return 0;
        }
        x++;
    }
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (p->free == 0)
        {
            p->free = 1;
            memset(p->filename, 0, 8);
            strcpy(p->filename, str);
            p->attribute = 0;
            p->length = 0;
            setFileTime(p, NULL);
            if (getFreeBlock(p) == 0)
            {
                printf("抱歉，磁盘空间不足，请扩容");
                return 0;
            }
            // printf("%d\n",p->first);
            fcb *q = (fcb *)(startp + BLOCKSIZE * (p->first));
            initFCB(q, p->first, NULL);
            initFCB(++q, 0, parent);
            return 1;
        }
        p++;
    }

    // 如果跑到这里了说明该文件目录下没fcb可以分配

    // 仅支持当前目录下创建
    return 0;
}

// 支持当前目录递归删除,但仅仅是当前目录
int rmdir(char *str, int mode)
{ // mode=1为递归删除，mode=0为普通删除  返回值为0说明失败
    if (str == NULL || str[0] == '\0')
    {
        printf("请输入要删除的文件夹名称\n");
        return 0;
    }
    if (isCurrentDir(str) == 0)
    {
        return 0;
    }
    fcb *p = (fcb *)(char *)currentBlock;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (p->attribute == 0 && strcmp(p->filename, str) == 0)
        {
            fcb *q = (fcb *)(startp + BLOCKSIZE * (p->first));
            if (mode == 0)
            {
                for (int j = 0; j < BLOCKSIZE / sizeof(fcb); j++)
                {
                    if (q->free == 1 && strcmp(q->filename, ".") != 0 && strcmp(q->filename, "..") != 0)
                    {
                        printf("%s\n", q->filename);
                        printf("%s下不为空，请清空后再操作\n", p->filename);
                        return 0;
                    }
                    q++;
                }
                // 如果跑到这里，说明对应文件夹目录下是空的，接下来就置为空标志符
                for (int j = 0; j < BLOCKSIZE / sizeof(fcb); j++)
                {
                    if (q->free == 1)
                    {
                        q->free = 0;
                    }
                    q++;
                }
                p->free = 0;
            }
            else if (mode == 1)
            {
                p->free = 0;
                rmAll(q);
            }
            break;
        }
        p++;
    }
    if (mode == 0)
    {
        printf("没找到 %s \n", str);
        return 0;
    }
    // mode=1为递归删除，mode=0为普通删除  返回值为0说明失败
    return 1;
}

void rmAll(struct FCB *F)
{ // 递归删除 有bug，不会删除打开文件的打开列表
    if (F == NULL)
    {
        return;
    }
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (F->free == 1 && F->attribute == 0 && strcmp(F->filename, ".") != 0 && strcmp(F->filename, "..") != 0)
        {
            F->free == 0;
            fcb *p = (fcb *)(startp + BLOCKSIZE * (F->first));
            rmAll(p);
        }
        else if (F->free == 1 && F->attribute == 1)
        {
            F->free = 0;
            fat *start = (fat *)startFAT, *p = (fat *)startFAT, *next = (fat *)startFAT;
            p += F->first;
            next = p;
            while (p->id != END)
            {
                next = start + p->id;
                p->id = FREE;
                p = next;
            }
            p->id = FREE;
        }
        else if (F->free == 1 && F->attribute == 0 && (strcmp(F->filename, ".") == 0 || strcmp(F->filename, "..") == 0))
        {
            F->free = 0;
        }
        F++;
    }
    return;
}

// 允许跨越目录打开文件
int openFile(char *str)
{ // 返回值0说明发生了错误
    char result[10][8];
    int length = split(str, result, '/');
    if (length == 0)
    {
        printf("@路径错误\n");
        return 0;
    }
    char newPath[80] = "";
    char filename[8] = "";
    char exname[4] = "";
    char a[2][8];
    if (length > 1)
    {
        length = spliteDot(result[length - 1], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
        recombination(str, result, length, newPath);
    }
    else
    {
        length = spliteDot(result[0], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        // printf("filename:%s\n",filename);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    // printf("%s %s\n",filename,exname);

    unsigned char *backupBlock = currentBlock;
    unsigned char backupcDir[80] = "";
    strcpy(backupcDir, currentdir); // 备份一下当前目录信息

    void back()
    {
        memset(currentdir, 0, 80);
        currentBlock = backupBlock;
        strcpy(currentdir, backupcDir);
    }

    if (cd(newPath, 0) == 0)
    {
        printf("@@路径错误\n");
        return 0;
    }
    if (length == 1)
    {
        strcpy(newPath, currentdir);
    }

    fcb *p = (fcb *)currentBlock;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        // printf("%s %s\n",p->filename,filename);
        if (p->free == 1 && p->attribute == 1 && strcmp(p->filename, filename) == 0 && strcmp(p->exname, exname) == 0)
        {
            // 到这里说明找到文件了
            useropen *q = openFileList;
            while (q != NULL)
            {
                // printf("%d %d %s %s %s %s\n",q->first,p->first,q->filename,filename,q->exname,exname);
                if (q->topenfile != 0 && strcmp(q->filename, filename) == 0 && strcmp(q->exname, exname) == 0 && q->first == p->first)
                {
                    printf("文件已打开\n");
                    back();
                    return 0;
                }
                q = q->next;
            }
            // 跑到这说明文件没打开过
            q = openFileList;
            while (q != NULL)
            {
                if (q->topenfile == 0)
                {
                    setUserOpen(p, q);
                    printf("文件打开成功\n");
                    printf("文件描述符:%d\n", q->fd);
                    back();
                    return 1;
                }
                q = q->next;
            }
            // 跑到这说明没有空的topenfile了
            q = openFileList->next;
            useropen *header = openFileList, *newHeader = q;
            openFileList = q;
            while (q->next != NULL)
            {
                q->header = newHeader;
                q = q->next;
            }
            q->header = newHeader;
            q->next = header;
            header->next = NULL;
            setUserOpen(p, header);
            printf("文件打开成功\n");
            back();
            return 1;
        }
        p++;
    }
    // 如果跑到这里，说明没找到对应的文件
    printf("文件不存在\n");
    back();
    return 0;
}

// 支持跨域新建文件
int createFile(char *str)
{
    char result[10][8];
    int length = split(str, result, '/');
    if (length == 0)
    {
        printf("@路径错误\n");
        return 0;
    }
    char newPath[80] = "";
    char filename[8] = "";
    char exname[4] = "";
    char a[2][8];
    if (length > 1)
    {
        recombination(str, result, length, newPath);
        length = spliteDot(result[length - 1], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    else
    {
        length = spliteDot(result[0], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        // printf("filename:%s\n",filename);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    // printf("@@%s\n",newPath);
    unsigned char *backupBlock = currentBlock;
    unsigned char backupcDir[80] = "";
    strcpy(backupcDir, currentdir); // 备份一下当前目录信息
    void back()
    {
        memset(currentdir, 0, 80);
        currentBlock = backupBlock;
        strcpy(currentdir, backupcDir);
    };

    if (cd(newPath, 0) == 0)
    {
        printf("@@路径错误\n");
        return 0;
    };
    if (length == 1)
    {
        strcpy(newPath, currentdir);
    };
    // printf("newPath:%s\n", newPath);
    // printf("filename:%s exname:%s\n", filename, exname);
    fcb *p = (fcb *)currentBlock;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (p->free == 1 && strcmp(p->filename, filename) == 0 && p->attribute == 1)
        {
            printf("文件已存在\n");
            back();
            return 0;
        }
        p++;
    }
    p = (fcb *)currentBlock;
    // 跑到这说明文件不存在
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (p->free == 0)
        {
            char result[2][8];
            if (length == 0)
            {
                printf("文件名或后缀名过长\n");
                back();
                return 0;
            }
            if (getFreeBlock(p) == 0)
            {
                printf("磁盘已满\n");
                back();
                return 0;
            }
            initNormalFCB(p, p->first, filename, exname);
            back();
            return 1;
        }
        p++;
    }
    // 跑到这说明没fcb可以分配了
    printf("%s目录下文件数量已满\n", newPath);
    back();
    return 0;
}

// 支持跨域删除
int removeFile(char *str)
{
    char result[10][8];
    int length = split(str, result, '/');
    if (length == 0)
    {
        printf("@路径错误\n");
        return 0;
    }
    char newPath[80] = "";
    char filename[8] = "";
    char exname[4] = "";
    char a[2][8];
    if (length > 1)
    {
        recombination(str, result, length, newPath);
        length = spliteDot(result[length - 1], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    else
    {
        length = spliteDot(result[0], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        // printf("filename:%s\n",filename);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }

    unsigned char *backupBlock = currentBlock;
    unsigned char backupcDir[80] = "";
    strcpy(backupcDir, currentdir); // 备份一下当前目录信息
    void back()
    {
        memset(currentdir, 0, 80);
        currentBlock = backupBlock;
        strcpy(currentdir, backupcDir);
    };

    if (cd(newPath, 0) == 0)
    {
        printf("@@路径错误\n");
        return 0;
    };
    if (length == 1)
    {
        strcpy(newPath, currentdir);
    };

    fcb *p = (fcb *)currentBlock;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (p->free == 1 && strcmp(p->filename, filename) == 0 && p->attribute == 1)
        {
            useropen *open = openFileList, *preT, *target, *tail;
            int flag = 0;
            // printf("%d %s %d\n",open->fd,open->filename,open->topenfile);
            while (open != NULL)
            {
                if (open->next == NULL)
                {
                    tail = open;
                }
                if (open->next != NULL && open->next->topenfile != 0 && open->next->first == p->first && flag == 0)
                {
                    preT = open;
                }
                if (open->topenfile != 0)
                {
                    if (flag == 0)
                    {
                        if (open->first == p->first)
                        {
                            target = open;
                            open->topenfile = 0;
                            flag = 1;
                        }
                    }
                }
                open = open->next;
            }
            // 下面是将对应的useropen放到末尾去
            if (flag == 1)
            {
                if (openFileList->first == p->first)
                {
                    useropen *q = openFileList->next;
                    useropen *header = openFileList, *newHeader = q;
                    while (q->next != NULL)
                    {
                        q->header = newHeader;
                        q = q->next;
                    }
                    q->header = newHeader;
                    q->next = header;
                    header->next = NULL;
                }
                else if (target->next != NULL)
                {
                    preT->next = target->next;
                    tail->next = target;
                    target->next = NULL;
                }
            }
            p->free = 0;
            int first = p->first;
            fat *f = (fat *)(startFAT);
            f += first;
            fat *q = f;
            while (f->id != END)
            {
                f = (fat *)(startFAT);
                f += f->id;
                q->id = FREE;
                q = f;
            }
            f->id = FREE;
            back();
            return 1;
        }
        p++;
    }
    // 跑到这说明文件不存在
    back();
    return 0;
}

// 支持跨域
int writeFile(char *str, int mode)
{ // mode=-1等于重来，mode=0等于覆盖 mode=1等于追加
    char result[10][8];
    int length = split(str, result, '/');
    if (length == 0)
    {
        printf("@路径错误\n");
        return 0;
    }
    char newPath[80] = "";
    char filename[8] = "";
    char exname[4] = "";
    char a[2][8];
    if (length > 1)
    {
        recombination(str, result, length, newPath);
        length = spliteDot(result[length - 1], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    else
    {
        length = spliteDot(result[0], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");

            return 0;
        }
        strcpy(filename, a[0]);
        // printf("filename:%s\n",filename);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    // printf("%s %s\n",filename,exname);

    unsigned char *backupBlock = currentBlock;
    unsigned char backupcDir[80] = "";
    strcpy(backupcDir, currentdir); // 备份一下当前目录信息
    void back()
    {
        memset(currentdir, 0, 80);
        currentBlock = backupBlock;
        strcpy(currentdir, backupcDir);
    }
    if (cd(newPath, 0) == 0)
    {
        printf("@@路径错误\n");
        return 0;
    }
    if (length == 1)
    {
        strcpy(newPath, currentdir);
    }
    fcb *p = (fcb *)currentBlock;
    // printf("下面进入open判断\n");
    useropen *open = openFileList;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        printf("%s %s\n", p->filename, filename);
        if (p->free == 1 && strcmp(p->filename, filename) == 0 && p->attribute == 1)
        {
            // printf("找到对应的FCB了");
            int flag = 0;
            while (open != NULL)
            {
                if (open->topenfile != 0 && open->first == p->first)
                {
                    flag = 1;
                    break;
                }
                open = open->next;
            }
            if (flag == 0)
            {
                printf("请先打开文件\n");
                back();
                return 0;
            }
            else
            {
                break;
            }
        }
        p++;
    }
    // printf("文件已经打开");
    fat *f = (fat *)startFAT;
    f += p->first;
    unsigned char *block = startp + BLOCKSIZE * (p->first);
    printf("请输入内容（EOF作为结束）符：\n");
    char buffer[BLOCKSIZE];
    int status = 0, index = 0;
    char ch;
    int over(int offset) // 结束判断逻辑
    {
        if (ch == 'E' && status == 0)
        {
            status = 1;
        }
        else if (ch == 'O' && status == 1)
        {
            status = 2;
        }
        else if (ch == 'F' && status == 2)
        {
            // printf("@@%s@@,%d\n",buffer,index);
            buffer[index - 3] = '\0';
            // printf("@@%s@@",buffer);
            strncpy(block + offset, buffer, strlen(buffer));
            strncpy(block + offset + strlen(buffer), "\0", 1);
            // printf("%d\n",strlen(buffer));
            p->length += strlen(buffer);
            // printf("%d\n", f->id);
            if (f->id != FREE && f->id != END)
            {
                int id;
                while (f->id != END)
                {
                    id = f->id;
                    f->id = FREE;
                    f = (fat *)(startFAT) + id;
                }
                f->id = FREE;
            }
            else if (f->id == FREE)
            { // 如果这个块是新分配的，那么其id就是free
                f->id = END;
            }
            getchar();
            // printf("@@@%s@@ %s@@\n",block,buffer);
            return 1;
        }
        else
        {
            status = 0;
        }
        // 0为继续，1为结束
        return 0;
    }
    int write() // 完全覆盖写入逻辑
    {
        buffer[index] = ch;
        index++;
        if (index == BLOCKSIZE - 1)
        {
            buffer[index] = '\0';
            strcpy(block, buffer);
            // printf("@@@%s@@ %s@@\n",block,buffer);
            index = 0;
            memset(buffer, 0, sizeof(buffer));
            p->length += BLOCKSIZE - 1;
            if (f->id == END)
            {
                int a = getFreeBlock(NULL);
                if (a == 0)
                {
                    printf("磁盘已满\n");
                    f->id = END;
                    return 0;
                }
                f->id = a;
                block = startp + BLOCKSIZE * (f->id);
                f = (fat *)startFAT;
                f += a;
            }
            else
            {
                block = startp + BLOCKSIZE * (f->id);
                f = (fat *)startFAT;
                f += f->id;
            }
        }
        if (over(0))
        {
            return 1;
        }
        // 2表示继续
        return 2;
    }
    if (mode == -1 || mode == 0 && open->count == 0) // 这是完全覆盖写
    {
        open->count = 0;
        while (ch = getchar())
        {
            int a = write();
            if (a != 2)
            {
                back();
                return a;
            }
        }
    }
    else if (mode == 0 || mode == 1)
    { // 从打开文件列表里面的count位开始写
        int offset = 0;
        if (mode == 1)
        {
            offset = p->length;
        }
        else if (mode == 1)
        {
            offset = open->count;
        }

        printf("offset:%d\n", offset);
        open->count = 0;
        p->length = 0;
        while (offset > BLOCKSIZE - 1)
        {
            block = startp + BLOCKSIZE * (f->id);
            f = (fat *)startFAT + f->id;
            offset -= BLOCKSIZE - 1;
            p->length += BLOCKSIZE - 1;
        }
        index = 0;
        memset(buffer, 0, sizeof(buffer));
        while (ch = getchar())
        {
            if (offset != 0)
            {
                buffer[index] = ch;
                index++;
                if (offset + index == 1023)
                {
                    buffer[index] = '\0';
                    strncpy(block + offset, buffer, index);
                    p->length += index;
                    if (f->id != END)
                    {
                        block = startp + BLOCKSIZE * (f->id);
                        f = (fat *)(startFAT) + f->id;
                    }
                    else
                    {
                        int a = getFreeBlock(NULL);
                        if (a == 0)
                        {
                            printf("磁盘已满\n");
                            return 0;
                        }
                        f->id = a;
                        block = startp + BLOCKSIZE * a;
                        f = (fat *)(startFAT) + a;
                        f->id = END;
                    }
                    offset = 0;
                    index = 0;
                    memset(buffer, 0, sizeof(buffer));
                }
                // printf("@%s@\n",buffer);
                if (over(offset))
                {
                    return 1;
                }
                continue;
            }
            // 如果能走到这里，说明上面offset的那个盘块已经被写满了，并且找到了接下来的一个盘块
            // 接下来的盘块的写入逻辑和mode=-1的一样
            // printf("开始读入后面的部分\n");
            int a = write();
            if (a != 2)
            {
                back();
                return a;
            }
        }
    }
}

// 支持跨域
int readFile(char *str, int len)
{ // 如果len为0 那么就全部读取
    char result[10][8];
    int length = split(str, result, '/');
    if (length == 0)
    {
        printf("@路径错误\n");
        return 0;
    }
    char newPath[80] = "";
    char filename[8] = "";
    char exname[4] = "";
    char a[2][8];
    if (length > 1)
    {
        recombination(str, result, length, newPath);
        length = spliteDot(result[length - 1], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    else
    {
        length = spliteDot(result[0], a);
        if (length == 0)
        {
            printf("文件名不符合格式\n");
            return 0;
        }
        strcpy(filename, a[0]);
        // printf("filename:%s\n",filename);
        if (length == 1)
        {
            strcpy(exname, "");
        }
        else
        {
            strcpy(exname, a[1]);
        }
    }
    // printf("%s %s\n",filename,exname);

    unsigned char *backupBlock = currentBlock;
    unsigned char backupcDir[80] = "";
    strcpy(backupcDir, currentdir); // 备份一下当前目录信息
    void back()
    {
        memset(currentdir, 0, 80);
        currentBlock = backupBlock;
        strcpy(currentdir, backupcDir);
    }
    if (cd(newPath, 0) == 0)
    {
        printf("@@路径错误\n");
        return 0;
    }
    if (length == 1)
    {
        strcpy(newPath, currentdir);
    }
    fcb *p = (fcb *)currentBlock;
    useropen *open = openFileList;
    for (int i = 0; i < BLOCKSIZE / sizeof(fcb); i++)
    {
        if (p->free == 1 && strcmp(p->filename, filename) == 0 && p->attribute == 1)
        {
            int flag = 0;
            while (open != NULL)
            {
                if (open->topenfile != 0 && open->first == p->first)
                {
                    flag = 1;
                    break;
                }
                open = open->next;
            }
            if (flag == 0)
            {
                printf("请先打开文件\n");
                back();
                return 0;
            }
            else
            {
                break;
            }
        }
        p++;
    }
    fat *f = (fat *)startFAT;
    f += p->first;
    unsigned char *block = startp + BLOCKSIZE * (p->first);
    open->count = 0;
    // printf("%s\n",block);
    char buffer[BLOCKSIZE];
    memset(buffer, 0, sizeof(buffer));
    if (len != 0)
    {
        if (len <= BLOCKSIZE - 1)
        {
            strncpy(buffer, block, len);
            open->count = len;
            printf("%s\n", buffer);
            back();
            return 1;
        }
        else
        {
            strncpy(buffer, block, BLOCKSIZE);
            printf("%s", buffer);
            if (f->id == END)
            {
                open->count = strlen(block);
                back();
                return 1;
            }
            else
            {
                len -= BLOCKSIZE - 1;
                open->count = 1023;
            }
        }
    }
    if (len == 0 && f->id == END)
    {
        strncpy(buffer, block, strlen(block));
        printf("%s\n", buffer);
        open->count = strlen(block);
        back();
        return 1;
    }
    else if (len == 0)
    {
        strncpy(buffer, block, BLOCKSIZE);
        printf("%s", buffer);
        open->count = BLOCKSIZE - 1;
    }

    while (f->id != END && len != 0)
    {
        memset(buffer, 0, sizeof(buffer));
        block = startp + BLOCKSIZE * (f->id);
        f = ((fat *)startFAT) + f->id;
        if (len > BLOCKSIZE - 1)
        {
            strncpy(buffer, block, BLOCKSIZE);
            open->count += BLOCKSIZE - 1;
            printf("%s", buffer);
            len -= BLOCKSIZE - 1;
            memset(buffer, 0, sizeof(buffer));
        }
        else
        {
            strncpy(buffer, block, len);
            open->count += len;
            printf("%s\n", buffer);
            back();
            return 1;
        }
    }
    while (f->id != END && len == 0)
    {
        memset(buffer, 0, sizeof(buffer));
        block = startp + BLOCKSIZE * (f->id);
        // printf("%s", block);
        f = ((fat *)startFAT) + f->id;
        strncpy(buffer, block, BLOCKSIZE);
        open->count += strlen(block);
        printf("%s", block);
    }
    back();
    printf("\n");
}

void exitsys()
{
    FILE *fp;
    fp = fopen(FILENAME, "w");
    fwrite(myvhard, SIZE, 1, fp);
    free(myvhard);
    fclose(fp);
}

void getHelp()
{
    printf("------------------------------------------------------------------------\n");
    printf(GREEN"[]为可选,<>必选,*支持跨域\n");
    printf("*ls         [路径名]            列出目录\n");
    printf("*cd         <路径名>            进入目录\n");
    printf("mkdir       <文件名名>          新建文件夹\n");
    printf("rmdir       <文件夹名>          删除文件夹\n");
    printf("rmdirAll    <文件夹名>          递归删除文件夹下所有文件\n");
    printf("*create     <文件名>            新建文件\n");
    printf("*open       <文件名>            打开文件(若文件表满则移除最久未打开的)\n");
    printf("*write      <文件名> [参数]     默认-w全部覆盖写 -r继上次读末尾写 -a末尾写\n");
    printf("*read       <文件名> [长度]     读取文件\n");
    printf("exit                           退出系统\n"NONE);
}

int main()
{
    startsys();
    char commond[20] = "";
    char result[3][80];
    int length = 0;
    int cmdLen = 0;
    while (1)
    {
        printf(LIGHT_PURPLE"%s>"NONE, currentdir);
        memset(result, 0, sizeof(result));
        memset(commond, 0, sizeof(commond));
        my_getline(commond, 20);
        // printf("%s\n",commond);
        // scanf("%s", commond);
        // getchar();
        length = splitSpaceKey(commond, result);
        // printf("%s\n", commond);
        // printf("@%d@ @%s@ @%s@ @%s@\n",length,result[0],result[1],result[2]);
        if (length == 0)
        {
            printf(RED"命令错误\n"NONE);
            continue;
        }
        if (strcmp(result[0], "ls") == 0)
        {
            if (length != 1)
            {
                ls(result[1]);
            }
            else
                ls("");
        }
        else if (strcmp(result[0], "cd") == 0)
        {
            cd(result[1], 1);
        }
        else if (strcmp(result[0], "mkdir") == 0)
        {
            // printf("%s\n",result[1]);
            mkdir(result[1]);
        }
        else if (strcmp(result[0], "rmdirAll") == 0)
        {
            rmdir(result[1], 1);
        }
        else if (strcmp(result[0], "rmdir") == 0)
        {
            rmdir(result[1], 0);
        }
        else if (strcmp(result[0], "create") == 0)
        {
            createFile(result[1]);
        }
        else if (strcmp(result[0], "write") == 0)
        {
            if (length >= 2)
            {
                if (strcmp(result[2], "-w") == 0 || length == 2)
                { // 完全重写
                    writeFile(result[1], -1);
                }
                else if (strcmp(result[2], "-r") == 0)
                { // 读到的地方续写
                    writeFile(result[1], 0);
                }
                else if (strcmp(result[2], "-a") == 0)
                { // 末尾续写
                    writeFile(result[1], 1);
                }
            }
            else
            {
                printf("命令无效\n");
            }
            // atoi(result[2])
        }
        else if (strcmp(result[0], "open") == 0)
        {
            openFile(result[1]);
        }
        else if (strcmp(result[0], "remove") == 0)
        {
            removeFile(result[1]);
        }
        else if (strcmp(result[0], "read") == 0)
        {
            if (length == 3 && atoi(result[2]))
            {
                readFile(result[1], atoi(result[2]));
            }
            else if (length == 2)
            {
                readFile(result[1], 0);
            }
            else
            {
                printf("命令无效\n");
            }
        }
        else if (strcmp(result[0], "exit") == 0)
        {
            exitsys();
            return 0;
        }
        else if (strcmp(result[0], "help") == 0)
        {
            getHelp();
        }
        else
        {
            printf(RED"命令不存在\n"NONE);
        }
    }
    // ls("");
    // printf("\n\n");
    // createFile("abc.txt");
    // mkdir("a");
    // createFile("/root/a/x.txt");
    // ls("");
    // createFile("./a/x.txt");
    // ls("a");
    // openFile("a/x.txt");
    // writeFile("a/x.txt", -1);
    // readFile("a/x.txt",0);
    // ls("a");
    // ls("");
    return 0;
}