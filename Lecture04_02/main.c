#include <stdio.h>

int* ReturnAdress(int* num){ 
	int* pointer;
	pointer = num;
	return pointer;
}
int PrintValue(int *pointer){ 
	printf("a변수의 값은 %d\n", *pointer);
	return 0;
}
int main(){
	int a = 0;
	PrintValue(&a);
	a = 99;
	PrintValue(ReturnAdress(&a));
	a = 87;
	PrintValue(ReturnAdress(&a));
	return 0;
}