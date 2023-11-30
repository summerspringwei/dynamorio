char buffer[128];
void print_buffer(char* buffer, long long size) {
    // Inline ARM64 assembly to print the buffer content
    asm volatile (
        "mov x8, #64\n"         // syscall: write
        "mov x0, #1\n"          // File descriptor: stdout
        "mov x2, %0\n"          // Size of the buffer
        "mov x1, %1\n"          // Buffer address
        "mov x16, #0\n"         // ARM64 Linux syscall instruction
        "svc #0\n"              // Make a system call
        :
        : "r" (size), "r" (buffer)
        : "x0", "x1", "x2", "x8", "x16"
    );
}
int main(int argc, char* argv[]){
    const int num_of_lines = 8;
    int idx = 0;
    for(int i=1; i<num_of_lines; i++){
        for(int j=0; j<i; ++j){
            buffer[idx++]='*';
        }
        buffer[idx++]='\n';
    }
    print_buffer(buffer, 128);
    return 0;
}