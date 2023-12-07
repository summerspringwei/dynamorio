
#if defined(AARCHXX)
unsigned long int get_arm_clock_freq(){
    unsigned long int freq = 0;
    asm __volatile__("mrs x1, CNTFRQ_EL0\n\t"
                    "add %0, x1, #0\n\t"
                    : "=r"(freq)
                    :);
    return freq;
}
#endif
