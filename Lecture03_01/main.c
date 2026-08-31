//문제 : 
//x,y,z 라는 변수가 있고 (정수형) 0이상 999이하의 숫자로만 이 세개값을 조합해서 
// x * y - z 가 특정 숫자가 되는 경우를 하나라도 발견했을때 프로그램을 종료하시오.

#include <stdio.h>

int main()
{
	int x, y, z, flag;
	x = 0;	y = 0;	z = 0; flag = 0;

	//x * y - z = 9675;
	for (x = 0; x < 999; x++)	{
		for (y = 0; y < 999; y++)	{
			for (z = 0; z < 999; z++)	{
				if ((x * y - z) == 9675)	{
					flag = 1;
					printf("찾았습니다 x = %d , y = %d , z = %d \n",x,y,z);
					printf("찾았습니다 %d * %d - %d = %d \n",x,y,z, x * y - z);
					break;
				}
			}
			if (flag)
				break;
		}
		if (flag)
			break;
	}

	return 0;
}