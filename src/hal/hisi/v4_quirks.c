int (*fnISP_AlgRegisterDehaze)(int);
int (*fnISP_AlgRegisterDrc)(int);
int (*fnISP_AlgRegisterLdci)(int);
int (*fnMPI_ISP_IrAutoRunOnce)(int, void*);

int ISP_AlgRegisterDehaze(int pipeId) {
    return fnISP_AlgRegisterDehaze(pipeId);
}
int ISP_AlgRegisterDrc(int pipeId) {
    return fnISP_AlgRegisterDrc(pipeId);
}
int ISP_AlgRegisterLdci(int pipeId) {
    return fnISP_AlgRegisterLdci(pipeId);
}
int MPI_ISP_IrAutoRunOnce(int pipeId, void *irAttr) {
    return fnMPI_ISP_IrAutoRunOnce(pipeId, irAttr);
}