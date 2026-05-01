#include <stdio.h>
#include <utils.h>
#include <subutils.h>

int main(){
    int a = 10;
    int b = 24;

    int c = addition(a, b);
    printf("%d + %d = %d\n", a, b, c);

    int d = substraction(c, a);
    printf("%d - %d = %d\n", c, a, d);
}
