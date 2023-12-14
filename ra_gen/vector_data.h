/* generated vector header file - do not edit */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H
#ifdef __cplusplus
        extern "C" {
        #endif
/* Number of interrupts allocated */
#ifndef VECTOR_DATA_IRQ_COUNT
#define VECTOR_DATA_IRQ_COUNT    (1)
#endif
/* ISR prototypes */
void r_icu_isr(void);

#if __has_include("r_ioport.h")
        /* Vector table allocations */
        #define VECTOR_NUMBER_ICU_IRQ5 ((IRQn_Type) 5) /* ICU IRQ5 (External pin interrupt 5) */
        #define ICU_IRQ5_IRQn          ((IRQn_Type) 5) /* ICU IRQ5 (External pin interrupt 5) */
        #endif

#ifdef __cplusplus
        }
        #endif
#endif /* VECTOR_DATA_H */
