#include <stdio.h> 
#include <unistd.h>


int i = 0;

int main() {
    while (i <= 0) {
        printf("Le paquet fonctionne \n");
        fflush(stdout);   
        sleep(3);
    }
    return 0;
}