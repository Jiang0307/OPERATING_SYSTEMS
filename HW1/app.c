#include <stdio.h>
#include <string.h>

char input[200],  version[2][100],  memory[11][100],  time[3][100];
int version_start_line, cpu_start_line, memory_start_line, time_start_line;
int version_index=0, cpu_index=0, memory_index=0, time_index=0;
int line=0,which=0;

int count_line()
{
    FILE *fp = fopen("/proc/my_info","r");
    while( fgets (input, 100, fp)!=NULL )
    {
        if(which==0 && input[0]=='=' && input[1]=='=' && input[2]=='=' && input[3]=='=' && input[4]=='=')
        {
            version_start_line = line;
            which++;
        }
        else if(which==1 && input[0]=='=' && input[1]=='=' && input[2]=='=' && input[3]=='=' && input[4]=='=')
        {
            cpu_start_line = line;
            which++;
        }
        else if(which==2 && input[0]=='=' && input[1]=='=' && input[2]=='=' && input[3]=='=' && input[4]=='=')
        {
            memory_start_line = line;
            which++;
        }
        else if(which==3 && input[0]=='=' && input[1]=='=' && input[2]=='=' && input[3]=='=' && input[4]=='=')
        {
            time_start_line = line;
            which++;
        }
        line++;
    }
    fclose(fp);
    return memory_start_line - cpu_start_line;
}

void read_in(char cpu[][100])
{
    FILE *fp = fopen("/proc/my_info","r");
    for(int i=0; i<line; i++)
    {
        if(i==1 || i==2)
            fgets(version[version_index++], 100, fp);
        else if(i>=4 && i<=memory_start_line-2)
            fgets(cpu[cpu_index++], 100, fp);
        else if(i>=memory_start_line && i<=time_start_line-2)
            fgets(memory[memory_index++], 100, fp);
        else if(i>=time_start_line && i<line)
            fgets(time[time_index++], 100, fp);
        else
            fgets(input, 100, fp);
    }
    fclose(fp);
}

void print_out(char c,char cpu[][100],int cpu_line)
{
    if(c == 'v')
    {
        char output[100] = "Version：";
        strcat(output,version[1]);
        printf("\n%s",output);
        printf("\n----------------------------------------------------------------\n\n");
    }
    else if(c == 'c')
    {
        printf("\n");
        printf("CPU information：\n");
        for(int i=1; i<cpu_line-1; i++)
            printf("%s",cpu[i]);
        printf("\n----------------------------------------------------------------\n\n");
    }
    else if(c == 'm')
    {
        printf("\n");
        printf("Memory information：\n");
        for(int i=1; i<11; i++)
            printf("%s",memory[i]);
        printf("\n----------------------------------------------------------------\n\n");
    }
    else if(c == 't')
    {
        printf("\n");
        printf("Time information：\n");
        for(int i=1; i<3; i++)
            printf("%s",time[i]);
        printf("\n----------------------------------------------------------------\n\n");
    }
    else if(c == 'a')
    {
        printf("\n");

        for(int i=0; i<2; i++)
            printf("%s",version[i]);
        printf("\n");

        for(int i=0; i<cpu_line-1; i++)
            printf("%s",cpu[i]);
        printf("\n");

        for(int i=0; i<11; i++)
            printf("%s",memory[i]);
        printf("\n");

        for(int i=0; i<3; i++)
            printf("%s",time[i]);
        printf("\n----------------------------------------------------------------\n\n");
    }

    return;
}

int main()
{
    int cpu_line = count_line();
    char cpu[cpu_line][100];

    read_in(cpu);

    do
    {
        printf("Which information do you want?\n");
        printf("Version(v),CPU(c),Memory(m),Time(t),All(a),Exit(e)?\n");

        fgets(input,100,stdin);

        if (input[0] == 'e')
            break;
        else if(input[0] == 'v')
            print_out('v',cpu,cpu_line);
        else if(input[0] == 'c')
            print_out('c',cpu,cpu_line);
        else if(input[0] == 'm')
            print_out('m',cpu,cpu_line);
        else if(input[0] == 't')
            print_out('t',cpu,cpu_line);
        else if(input[0] == 'a')
            print_out('a',cpu,cpu_line);
    }
    while (input[0] != 'e');

    return 0;
}