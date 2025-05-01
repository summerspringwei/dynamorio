
// #include <stdio.h>
// #include <stdlib.h>

// int count_ones(int n){
//     int count = 0;
//     while(n){
//         count += n & 1;
//         n >>= 1;
//     }
//     return count;
// }

// int main(int argc, char *argv[]) {
//     int n = 0;
//     if(argc > 1){
//         n = atoi(argv[1]);
//     }
//     if(n == 0){
//         printf("Hello, world!\n");
//     }else{
//         printf("number of 1s in %d is %d\n", n, count_ones(n));
//     }
//     return 0;
// }

int main(int argc, char *argv[]) {
    int n = 0;
    if(argc > 1){
        n = atoi(argv[1]);
    }
    printf("n is %d\n", n);
    n = n + 1;
    printf("n is %d\n", n);
    return 0;
}