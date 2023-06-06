#ifndef INCLUDED_kernel_scauses_h
#define INCLUDED_kernel_scauses_h

#ifdef __cplusplus
extern "C" {
#endif

#define SCAUSE_IAM 0
#define SCAUSE_IAF 1
#define SCAUSE_II  2
#define SCAUSE_BR  3
#define SCAUSE_RESERVED_1 4
#define SCAUSE_LAF 5
#define SCAUSE_AAM 6
#define SCAUSE_ST_AMO_AF 7
#define SCAUSE_ECALL 8
#define SCAUSE_RESERVED_2 9 ... 11
#define SCAUSE_IPF 12
#define SCAUSE_LOAD_PF 13
#define SCAUSE_RESERVED_3 14
#define SCAUSE_ST_AMO_PF 15

#ifdef __cplusplus
}
#endif

#endif