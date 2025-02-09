#include <stdio.h>
#include <omp.h>
#include "mmio.h"
#include "time.h"

#ifdef LIKWID_MEASURE
#include <likwid.h>
#endif
#include "parse.h"
#include "sparsemat.h"
#include "densemat.h"
#include "kernels.h"
#include "timer.h"
#include <iostream>

void capitalize(char* beg)
{
    int i = 0;
    while(beg[i])
    {
        beg[i] = toupper(beg[i]);
        ++i;
    }
}

#define PERF_RUN(kernel, flopPerNnz)\
{\
    int iter = param.iter;\
    double time = 0;\
    double nnz_update = ((double)mat->nnz)*iter*1e-9;\
    sleep(1);\
    INIT_TIMER(kernel);\
    START_TIMER(kernel);\
    kernel(b, mat, x, iter);\
    STOP_TIMER(kernel);\
    time = GET_TIMER(kernel);\
    char* capsKernel;\
    asprintf(&capsKernel, "%s", #kernel);\
    capitalize(capsKernel);\
    printf("%10s : %8.4f GFlop/s ; Time = %8.5f s\n", capsKernel, flopPerNnz*nnz_update/(time), time);\
    free(capsKernel);\
}\

int findMetricId(int group_id, std::string toFind)
{
#ifdef LIKWID_PERFMON
    int numMetrics = perfmon_getNumberOfMetrics(group_id);

    int dataVol_metric_id = numMetrics-1;
    for(int i=0; i<numMetrics; ++i)
    {
        std::string currMetric(perfmon_getMetricName(group_id,i));
        std::cout<<"currMetric = "<<currMetric<<std::endl;
        if(currMetric.find(toFind) != std::string::npos)
        {
            dataVol_metric_id = i;
        }
    }

    return dataVol_metric_id;
#endif
}

void init_likwid()
{
#ifdef LIKWID_PERFMON
    int nthreads;
#pragma omp parallel
    {
#pragma omp single
        nthreads = omp_get_num_threads();
    }

    topology_init();
    CpuTopology_t topo = get_cpuTopology();
    affinity_init();

    int *cpus = (int*)malloc(nthreads * sizeof(int));

    for(int i=0;i<nthreads;i++)
    {
        cpus[i] = topo->threadPool[i].apicId;
    }

    perfmon_init(nthreads, cpus);

    int gid = perfmon_addEventSet("MEM");
    perfmon_setupCounters(gid);
    int mem_metric_id = findMetricId(gid, "Memory data volume\n");
    free(cpus);
#endif
}

void finalize_likwid()
{
#ifdef LIKWID_PERFMON
    //LIKWID_MARKER_CLOSE;
    perfmon_finalize();
    affinity_finalize();
    topology_finalize();
#endif
}

int main(const int argc, char * argv[])
{
//    init_likwid();
#ifdef LIKWID_PERFMON
    LIKWID_MARKER_INIT;
#endif
    int err;
    parser param;
    if(!param.parse_arg(argc, argv))
    {
        printf("Error in reading parameters\n");
    }
    sparsemat* mat = new sparsemat;

    printf("Reading matrix file\n");
    if(!mat->readFile(param.mat_file))
    {
        printf("Error in reading sparse matrix file\n");
    }
    printf("Preparing matrix for power calculation\n");

    int power = param.iter;
    printf("power = %d\n", power);
/*
#ifdef LIKWID_PERFMON
#pragma omp parallel
    {
        LIKWID_MARKER_START("pre_process");
    }
#endif*/
    mat->doRCM();
    int block_size = 2;
    mat->convertToBCSR(block_size);
    mat->prepareForPower(power, param.cache_size, param.cores, param.smt, param.pin);

/*#ifdef LIKWID_PERFMON
#pragma omp parallel
    {
        LIKWID_MARKER_STOP("pre_process");
    }
#endif*/

    //mat->writeFile("matrixAfterProcessing.mtx");
    printf("Finished preparing\n\n");

    int NROWS = mat->nrows*block_size;

    densemat *x, *xExact;

    double initVal = 1/(double)NROWS;
    //x stores value in the form
    //   x[0],   x[1], ....,   x[nrows-1]
    //  Ax[0],  Ax[1], ....,  Ax[nrows-1]
    // AAx[0], AAx[1], ...., AAx[nrows-1]
    densemat* xRACE=new densemat(NROWS, power+1);
    densemat* xRACE_0 = xRACE->view(0,0);
    xRACE_0->setVal(initVal);

    printf("calculation started\n");

    //determine iterations
    INIT_TIMER(matPower_init);
    START_TIMER(matPower_init);
    for(int iter=0; iter<10; ++iter)
    {
        matPowerBCSR(mat, power, xRACE);
    }
    STOP_TIMER(matPower_init);
    double initTime = GET_TIMER(matPower_init);
    int iterations = (int) (1.2*10/initTime);
    //int iterations = 1; //for correctness checking
    printf("Num iterations =  %d\n", iterations);

    double flops = 2.0*power*iterations*(double)mat->nnz*1e-9;

    densemat* xTRAD = NULL;
    if(param.validate)
    {
        xTRAD=new densemat(NROWS, power+1);
        densemat* xTRAD_0 = xTRAD->view(0,0);

        xTRAD_0->setVal(initVal);

        INIT_TIMER(spmvPower);
#ifdef LIKWID_PERFMON
#pragma omp parallel
        {
            LIKWID_MARKER_START("SpMV_power");
        }
#endif
        START_TIMER(spmvPower);
        //now calculate xTRAD in traditional way
        for(int iter=0; iter<iterations; ++iter)
        {
            for(int pow=0; pow<power; ++pow)
            {
                densemat *x = xTRAD->view(pow,pow);
                plain_spmv(mat, x);
            }
        }
        STOP_TIMER(spmvPower);
#ifdef LIKWID_PERFMON
#pragma omp parallel
        {
            LIKWID_MARKER_STOP("SpMV_power");
        }
#endif
        double spmvPowerTime = GET_TIMER(spmvPower);
        printf("SpMV power perf. = %f, time = %f\n", flops/spmvPowerTime, spmvPowerTime);

    }


    INIT_TIMER(matPower);
#ifdef LIKWID_PERFMON
#pragma omp parallel
    {
        LIKWID_MARKER_START("RACE_power");
    }
#endif
    START_TIMER(matPower);
    for(int iter=0; iter<iterations; ++iter)
    {
        matPowerBCSR(mat, power, xRACE);
    }
    /*for(int pow=0; pow<power; ++pow)
      {
      densemat *b = xRACE->view(pow+1,pow+1);
        densemat *x = xRACE->view(pow,pow);
        plain_spmv(b, mat, x, 1);
    }*/
    STOP_TIMER(matPower);
#ifdef LIKWID_PERFMON
#pragma omp parallel
    {
        LIKWID_MARKER_STOP("RACE_power");
    }
#endif
    double RACEPowerTime = GET_TIMER(matPower);
    printf("RACE power perf. = %f, time = %f\n", flops/RACEPowerTime, RACEPowerTime);


    if(param.validate)
    {
        bool flag = checkEqual(xTRAD, xRACE, param.tol);
        if(!flag)
        {
            printf("Power calculation failed\n");
        }
        else
        {
            printf("Power calculation success\n");
        }
        delete xTRAD;
    }

    delete mat;
    delete xRACE;
#ifdef LIKWID_PERFMON
    LIKWID_MARKER_CLOSE;
#endif
}
