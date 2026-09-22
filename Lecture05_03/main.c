#include <stdio.h>

int drawMoonRabbit()
{
    printf("\n");
    printf(" ()()\n");
    printf(" ( -.-)\n");
    printf("(>     )>\n");
    printf("  U  U   \n");
    printf(" 달토끼님  \n");

    return 0;
}

int main()
{
    drawMoonRabbit();

    char MoonRabbit[][20] = {
        "\n",
        " ()()\n",
        " ( -.-)\n",
        "(>     )>\n",
        "  U  U   \n",
        " 달토끼님  \n"
    };

    int i = 0;
    int j = 0;

    int row = sizeof(MoonRabbit) / sizeof(MoonRabbit[0]);
    int x = 5;
    int y = 3;

   
    for (i = 0;i < y;i++)
    {
        printf("\n");
    }

    for (i = 0;i < row;i++)
    {
        
        for (j = 0;j < y;j++)
        {
            printf(" ");
        }

        printf("%s", MoonRabbit[i]);
    }

    return 0;
}