#ifdef __arm__

void* (*fnIsp_Malloc)(unsigned long);
int   (*fnISP_AlgRegisterAcs)(int);
int   (*fnISP_AlgRegisterDehaze)(int);
int   (*fnISP_AlgRegisterDrc)(int);
int   (*fnISP_AlgRegisterLdci)(int);
int   (*fnMPI_ISP_IrAutoRunOnce)(int, void*);

void *isp_malloc(unsigned long size) {
    return fnIsp_Malloc(size);
}
int isp_alg_register_acs(int pipeId) {
    return fnISP_AlgRegisterAcs(pipeId);
}
int isp_alg_register_dehaze(int pipeId) {
    return fnISP_AlgRegisterDehaze(pipeId);
}
int ISP_AlgRegisterDehaze(int pipeId) {
    return fnISP_AlgRegisterDehaze(pipeId);
}
int isp_alg_register_drc(int pipeId) {
    return fnISP_AlgRegisterDrc(pipeId);
}
int ISP_AlgRegisterDrc(int pipeId) {
    return fnISP_AlgRegisterDrc(pipeId);
}
int isp_alg_register_ldci(int pipeId) {
    return fnISP_AlgRegisterLdci(pipeId);
}
int ISP_AlgRegisterLdci(int pipeId) {
    return fnISP_AlgRegisterLdci(pipeId);
}
int isp_ir_auto_run_once(int pipeId, void *irAttr) {
    return fnMPI_ISP_IrAutoRunOnce(pipeId, irAttr);
}
int MPI_ISP_IrAutoRunOnce(int pipeId, void *irAttr) {
    return fnMPI_ISP_IrAutoRunOnce(pipeId, irAttr);
}

#endif