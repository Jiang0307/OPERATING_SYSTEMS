#include "MemManager.h"
#define MAX_PROCESSES 25
#define TLB_SIZE 32
#define DISK_SIZE 4096

// sysconfig
char last_process = -1;
char TLB_REPLACEMENT_POLICY[10], PAGE_REPLACEMENT_POLICY[10], FRAME_ALLOCATION_POLICY[10];
int PROCESSES, PAGES, FRAMES;
int TLB_index=0;
int first_line = 0;
// structure
int DISK[DISK_SIZE];
int *MEMORY;
int first_time = 0;
TLB tlb[TLB_SIZE];
REPLACEMENT *replacement_heads_local[MAX_PROCESSES], *replacement_tails_local[MAX_PROCESSES];
REPLACEMENT *replacement_head_global, *replacement_tail_global;

PAGE *page_tables[MAX_PROCESSES];
FILE *output;
// for analysis.txt
int TLB_hit_count[MAX_PROCESSES], PAGE_fault_count[MAX_PROCESSES], TLB_lookup_count[MAX_PROCESSES], PAGE_reference_count[MAX_PROCESSES];

void read_config()
{
    FILE *fp = fopen("./sys_config.txt","r");
    for (int i = 0 ; i<6 ; i++)
    {
        char line[1024];
        fgets(line, 100, fp);
        if(i==0)
        {
            char temp1[1024], temp2[1024], temp3[1024];
            sscanf(line, "%s %s %s %s", temp1, temp2, temp3, TLB_REPLACEMENT_POLICY);
        }
        else if(i==1)
        {
            char temp1[1024], temp2[1024], temp3[1024];
            sscanf(line, "%s %s %s %s", temp1, temp2, temp3, PAGE_REPLACEMENT_POLICY);
        }
        else if(i==2)
        {
            char temp1[1024], temp2[1024], temp3[1024];
            sscanf(line, "%s %s %s %s", temp1, temp2, temp3, FRAME_ALLOCATION_POLICY);
        }
        else if(i==3)
        {
            char temp1[1024], temp2[1024], temp3[1024], temp4[1024];
            sscanf(line, "%s %s %s %s", temp1, temp2, temp3, temp4);
            PROCESSES = atoi(temp4);
        }
        else if(i==4)
        {
            char temp1[1024], temp2[1024], temp3[1024], temp4[1024], temp5[1024];
            sscanf(line, "%s %s %s %s %s", temp1, temp2, temp3, temp4, temp5);
            PAGES = atoi(temp5);
        }
        else if(i==5)
        {
            char temp1[1024], temp2[1024], temp3[1024], temp4[1024], temp5[1024];
            sscanf(line, "%s %s %s %s %s", temp1, temp2, temp3, temp4, temp5);
            FRAMES = atoi(temp5);
        }
    }
    fclose(fp);
    //printf("%s %s %s %d %d %d\n", TLB_REPLACEMENT_POLICY, PAGE_REPLACEMENT_POLICY, FRAME_ALLOCATION_POLICY, PROCESSES, PAGES, FRAMES);
    return;
}

int convert_index(char c)
{
    int result;
    result = c - 'A';
    return result;
}

int find_free_frame()
{
    for (int i = 0; i < FRAMES; i++)
    {
        if(MEMORY[i] == -1)
        {
            //printf("free index = %d\n", i);
            MEMORY[i] = 1;
            return i;
        }
    }
    return -1;
}

int find_free_disk()
{
    int result = -1;
    for (int i = 0; i < DISK_SIZE; i++)
    {
        if(DISK[i] == -1)
        {
            //DISK[i] = page_tables[p->process][p->number].PFN_DBI;
            result = i;
            break;
        }
    }
    return result;
}

REPLACEMENT* DEQUEUE(REPLACEMENT** QUEUE_HEAD) //POP HEAD
{
    if(*QUEUE_HEAD == NULL) // 不可能會是NULL，只有frame是滿的才會呼叫這個，不用處理邊界條件
        return NULL;

    REPLACEMENT* current_head = *QUEUE_HEAD;
    REPLACEMENT* new_head = current_head->next;
    *QUEUE_HEAD = new_head;
    current_head->next = NULL;
    //printf("VPN : %d\n",current_head->VPN);
    return current_head;
}

REPLACEMENT *add_page(REPLACEMENT **head, REPLACEMENT **tail, char process, int VPN)  //是local也只要傳local head和local tail就好
{
    //printf("add new page\n");
    REPLACEMENT *new_node = malloc(sizeof(REPLACEMENT));
    new_node->process = process;
    new_node->VPN = VPN;
    new_node->reference = 0;
    new_node->next = NULL;
    //printf("ENTER PAGE ADD , VPN = %d\n",VPN);

    if(strcmp(PAGE_REPLACEMENT_POLICY,"FIFO")==0) // FIFO
    {
        if((*head) == NULL) // replacement list是空的
        {
            (*head) = new_node;
            (*tail) = new_node;
        }
        else
        {
            (*tail)->next = new_node;
            (*tail) = new_node;
        }
    }
    else // CLOCK
    {
        if((*head) == NULL) // replacement list是空的
        {
            (*head) = new_node;
            (*tail) = new_node;
            (*tail)->next = (*head);
        }
        else // 根據FIFO的原則，直接加在尾端即可。
        {
            (*tail)->next = new_node;
            (*tail) = new_node;
            (*tail)->next = (*head);
        }
    }

    return new_node;
}

void page_out(REPLACEMENT **head, REPLACEMENT **tail, char process, int VPN)
{
    REPLACEMENT *removed;
    int current_process = convert_index(process);
    //printf("ENTER PAGE OUT\n");
    if( strcmp(PAGE_REPLACEMENT_POLICY,"FIFO") == 0 ) // FIFO 把頭移除
    {
        removed = DEQUEUE(&(*head));
    }
    else // Clock 找到reference bit是0的移走
    {
        REPLACEMENT *previous = NULL;
        REPLACEMENT *current = (*head);
        while(current->reference == 1)
        {
            previous = current;
            current->reference = 0;
            current = current->next;
        }
        removed = current;

        if(current == *head)         // 目標在佇列頭
        {
            *head = (*head)->next;
            if(*head == NULL)
                *tail = NULL;
            else
                (*tail)->next = (*head);
            current->next = NULL;
        }
        else if(current == *tail)       //目標在佇列尾
        {
            *tail = previous;
            (*tail)->next = *head;
            current->next = NULL;
        }
        else
        {
            *head = current->next;
            *tail = previous;
            previous->next = *head;
            current->next = NULL;
        }
    }

    if(removed->process == process)
    {
        int page = removed->VPN;
        for (int i = 0; i < TLB_SIZE; i++)
        {
            if(tlb[i].VPN == page) // TLB HIT
            {
                int index;
                //TLB temp = tlb[i];
                tlb[i].PFN = -1;
                tlb[i].VPN = -1;
                for (index = i; index < TLB_SIZE-1; index++)
                {
                    if( (tlb[index+1].VPN == -1) && (tlb[index+1].PFN == -1) ) // 有空的entry
                        break;
                    tlb[index] = tlb[index + 1];
                }
                //tlb[index] = temp;
                tlb[TLB_SIZE - 1].VPN = -1;
                tlb[TLB_SIZE - 1].PFN = -1;
                TLB_index--;
                //break;
                printf("REMOVE %d FROM TLB\n",page);
                for (int i = 0; i < 32; i++)
                    printf("TLB[%d] = %d\n", i, tlb[i].VPN);
            }
        }
    }
    int output_PFN, output_victim, output_dest, output_VPN, output_src;
    char output_process;

    // 把剛移出的entry移入到Disk中
    int free_disk_index = find_free_disk();
    DISK[free_disk_index] = page_tables[convert_index(removed->process)][removed->VPN].PFN_DBI;

    if(page_tables[current_process][VPN].present == 0) // 若Paging in的Page是存在Disk裡面的
    {
        output_src = page_tables[current_process][VPN].PFN_DBI;
        DISK[output_src] = -1;
    }
    else                            // 否則代表這個PAGE是新的
    {
        output_src = -1;
    }
    page_tables[current_process][VPN].PFN_DBI = page_tables[convert_index(removed->process)][removed->VPN].PFN_DBI;
    page_tables[current_process][VPN].present = 1;
    page_tables[current_process][VPN].ptr = add_page(head, tail, process, VPN);

    output_process = removed->process;
    output_PFN = page_tables[current_process][VPN].PFN_DBI;
    output_victim = removed->VPN;
    output_VPN = VPN;

    page_tables[convert_index(removed->process)][removed->VPN].PFN_DBI = free_disk_index;
    page_tables[convert_index(removed->process)][removed->VPN].present = 0;
    page_tables[convert_index(removed->process)][removed->VPN].ptr = NULL;
    output_dest = free_disk_index;
    printf("Page Fault, %d, Evict %d of Process %c to %d, %d<<%d\n", output_PFN, output_victim, output_process, output_dest, output_VPN, output_src);
    fprintf(output,"Page Fault, %d, Evict %d of Process %c to %d, %d<<%d\n", output_PFN, output_victim, output_process, output_dest, output_VPN, output_src);
    free(removed);
}

void page_fault_handler(char process, int VPN)
{
    int current_process = convert_index(process);
    int free_frame_index = find_free_frame();
    if(free_frame_index == -1) // 沒有free frame，找victim
    {
        // printf("NO FREE FRAME\n");
        if( strcmp(FRAME_ALLOCATION_POLICY, "GLOBAL") == 0 )
            page_out( &replacement_head_global, &replacement_tail_global, process, VPN);
        else
            page_out( &replacement_heads_local[current_process], &replacement_tails_local[current_process], process, VPN);
    }
    else // 還有free frame，按照allocation policy做事
    {
        //printf("HAVE FREE FRAME : %d\n",free_frame_index);

        page_tables[current_process][VPN].PFN_DBI = free_frame_index;
        page_tables[current_process][VPN].present = 1;

        if( strcmp(FRAME_ALLOCATION_POLICY, "GLOBAL") == 0 )
            page_tables[current_process][VPN].ptr = add_page( &replacement_head_global,  &replacement_tail_global, process, VPN);
        else
            page_tables[current_process][VPN].ptr = add_page( &replacement_heads_local[current_process],  &replacement_tails_local[current_process], process, VPN);

        printf("Page Fault, %d, Evict -1 of Process %c to -1, %d<<-1\n", free_frame_index, process, VPN);
        fprintf(output, "Page Fault, %d, Evict -1 of Process %c to -1, %d<<-1\n", free_frame_index, process, VPN);
    }
}

void check_TLB(char process, int page)
{
    // printf("CHECK TLB , process : %c , page : %d\n",process,page);
    int current_process = convert_index(process);
    TLB_lookup_count[current_process]++;
    PAGE_reference_count[current_process]++;
    // 先看有沒有換process
    if(current_process != last_process)
    {
        // 重設 TLB
        TLB_index = 0;
        for (int i = 0; i < TLB_SIZE; i++)
        {
            tlb[i].PFN = -1;
            tlb[i].VPN = -1;
        }
        printf("CHANGE PROCESS to %c\n",process);
    }
    last_process = current_process;
    // 找TLB中有沒有該process的該page
    int count = 0;
    for (int i = 0; i < TLB_SIZE; i++)
    {
        //printf("TLB[%d].VPN = %d , VPN = %d\n", i, tlb[i].VPN,page);
        if(tlb[i].VPN == page ) // TLB HIT
        {
            printf("Process %c, TLB Hit, %d=>%d\n", process, page, page_tables[current_process][page].PFN_DBI);
            fprintf(output,"Process %c, TLB Hit, %d=>%d\n", process, page, page_tables[current_process][page].PFN_DBI);
            page_tables[current_process][page].ptr->reference = 1;
            TLB_hit_count[current_process]++;
            if( strcmp(TLB_REPLACEMENT_POLICY,"LRU")==0 ) // 更新TLB，把TLB hit的entry移動到TLB最下面
            {
                int index;
                int vpn = tlb[i].VPN;
                int pfn = tlb[i].PFN;
                for (index = i; index < TLB_SIZE-1; index++)
                {
                    if( (tlb[index+1].VPN == -1) && (tlb[index+1].PFN == -1) ) // 有空的entry
                        break;
                    tlb[index] = tlb[index + 1];
                }
                tlb[index].VPN = vpn;
                tlb[index].PFN = pfn;
            }
            break;
        }
        count++;
    }
    // printf("count = %d\n", count);
    if(count < TLB_SIZE)
    {
        // printf("TLB HIT\n");
        return;
    }
    else // TLB MISS
    {
        // printf("%d\n", __LINE__);
        if(page_tables[current_process][page].present == 1) // PAGE HIT
        {
            printf("Process %c, TLB Miss, ", process);
            printf("Page Hit, %d=>%d\n",page, page_tables[current_process][page].PFN_DBI);
            fprintf(output,"Process %c, TLB Miss, ", process);
            fprintf(output,"Page Hit, %d=>%d\n",page, page_tables[current_process][page].PFN_DBI);
            page_tables[current_process][page].ptr->reference = 1;
        }
        else if(page_tables[current_process][page].PFN_DBI == -1) // PAGE Fault
        {
            printf("Process %c, TLB Miss, ", process);
            fprintf(output,"Process %c, TLB Miss, ", process);
            page_fault_handler(process, page);
            PAGE_fault_count[current_process]++;
        }
        else // 代表Pages資訊在Disk裡的Page Fault
        {
            //printf("%d\n", __LINE__);
            printf("Process %c, TLB Miss, ", process);
            fprintf(output,"Process %c, TLB Miss, ", process);
            page_fault_handler(process, page);
            PAGE_fault_count[current_process]++;
        }
        // 更新TLB，判斷TLB滿了沒，滿了的話要用replacement algorithm，不然就直接插到尾端
        if(TLB_index < TLB_SIZE)
        {
            tlb[TLB_index].VPN = page;
            tlb[TLB_index].PFN = page_tables[current_process][page].PFN_DBI;
            printf("TLB have space at %d\n",TLB_index);
            TLB_index++;
        }
        else
        {
            if( strcmp(TLB_REPLACEMENT_POLICY,"LRU")==0 ) // 滿了，最上面的移除，新的插在最下面
            {
                TLB temp;
                temp.VPN = page;
                temp.PFN = page_tables[current_process][page].PFN_DBI;
                for (int index = 0; index < TLB_SIZE - 1; index++)
                {
                    tlb[index] = tlb[index + 1];
                }
                tlb[TLB_SIZE-1] = temp;
            }
        }
        check_TLB(process, page);
        PAGE_reference_count[current_process]--;
    }
}

void read_trace()
{
    FILE *fp = fopen("./trace.txt","r");
    char line[1024];
    while( fgets (line, 100, fp)!=NULL )
    {
        //讀資料
        char which_process, temp[1024];
        int which_page;
        sscanf(line,"%s %d",temp,&which_page);
        which_process = line[10];
        //printf("%c %d\n",which_process,which_page);
        //開始做事
        check_TLB(which_process, which_page);
    }
    fclose(fp);
    fclose(output);
    return;
}

void initialize()
{
    TLB_index = 0;
    for (int i = 0; i < PROCESSES; i++)
    {
        page_tables[i] = malloc(sizeof(PAGE) * PAGES);
        for (int j = 0; j < PAGES; j++)
        {
            page_tables[i][j].ptr = NULL;       // 指向replacement list對應的page
            page_tables[i][j].PFN_DBI = -1;
            page_tables[i][j].present = 0;      // 在不再MEMORY
        }
        replacement_heads_local[i] = NULL;
        replacement_tails_local[i] = NULL;
    }
    replacement_head_global = NULL;
    replacement_tail_global = NULL;

    MEMORY = (int *)malloc(sizeof(int) * FRAMES);
    for(int i=0; i<FRAMES; ++i)
        MEMORY[i] = -1;

    for(int i=0; i<DISK_SIZE; ++i)
        DISK[i] = -1;
    return;
}

void trace_output()
{
    FILE *read_in = fopen("output_temp.txt", "r");
    FILE *fp = fopen("trace_output.txt","w");
    char line[1024];
    int lines = 0, count = 0;
    while( fgets(line, 100, read_in)!=NULL )
    {
        lines++;
    }
    fclose(read_in);
    FILE* read_in2 = fopen("output_temp.txt", "r");
    while( fgets(line, 100, read_in2)!=NULL )
    {
        if(count == lines-1)
        {
            line[strlen(line) - 1] = '\0';
        }
        fprintf(fp,"%s",line);
        count++;
    }
    fclose(fp);
    // REMOVE output_temp.txt
    if( access( "output_temp.txt", F_OK ) == 0 )
    {
        char cmd[32];
        sprintf(cmd, "rm -rf %s", "output_temp.txt");
        int ret = system(cmd);
        ret = ret;
    }
    fclose(read_in2);
    return;
}

void analysis()
{
    FILE *fp = fopen("analysis.txt","w");
    for(int i=0 ; i<PROCESSES ; i++)
    {
        float hit_ratio = (float)TLB_hit_count[i] / (float)TLB_lookup_count[i];
        float TLB_lookup_time = 20;
        float memory_cycle_time = 100;
        float EAT = (hit_ratio*(TLB_lookup_time+memory_cycle_time)) + ((1 - hit_ratio) * ( (2 * memory_cycle_time) + TLB_lookup_time));
        fprintf(fp, "Process %c, Effective Access Time = %0.3f\n", 'A'+i,EAT);
        if(i == PROCESSES-1)
        {
            fprintf(fp, "Process %c, Page Fault Rate: %0.3f", 'A'+i, (float)PAGE_fault_count[i] / (float)PAGE_reference_count[i]);
        }
        else
        {
            fprintf(fp, "Process %c, Page Fault Rate: %0.3f\n", 'A'+i, (float)PAGE_fault_count[i] / (float)PAGE_reference_count[i]);
        }
    }
    fclose(fp);
}

int main()
{
    output = fopen("trace_output.txt", "w");
    read_config();
    initialize();
    read_trace();
    analysis();
    return 0;
}