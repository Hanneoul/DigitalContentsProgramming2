/*
 * 복습용 실습!
 * Character를 이동시켜보자!
 * getch()를 써서 a,d 키를 눌러 플레이어 캐릭터를 이동시켜볼 생각이다.
 * while문과 if문을 조합해서 printf로 플레이어 캐릭터 '*'을 이동시켜주세요.
 */


#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

int main()
{
	int positionX = 5; //5를 중앙기준으로 하며, 0~10까지만 이동가능하게 만든다.
	char key = 0;

	printf("시작하려면 아무키나 입력해 주세요!\n");

	while (key != 'q')
	{
		key = _getch();
		if (key == 'a')
		{
			if (positionX > 0)
				positionX = positionX - 1;
		}

		if (key == 'd')
		{ 
			if(positionX < 10)
				positionX = positionX + 1;
		}
		system("cls"); //화면을 지움

		int counter = 0;
		while(counter < positionX)
		{ 
			printf(" "); //공백
			counter = counter + 1;
		}
		printf("*"); //플레이어
	}
	
	return 0;
}