#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "parse.h"

my_option::my_option(const char* name_, int has_arg_, int* flag_, int val_, char*desc_): desc(desc_)
{
    gnu_opt = {name_, has_arg_, flag_, val_};
}

my_option::my_option()
{
}

parser::parser():mat_file(NULL), iter(-1), cores(1), smt(1), cache_size(2), pin(FILL), validate(false), tol(1e-4), convTol(1e-16), RCM_flag(false), colorType("RACE"), prgname("a.out"), convFile(NULL), numOptions(13)
{
    long_options = new my_option[numOptions+1];

    long_options[0] = {"matrix",  required_argument, 0,  'm', "Matrix File in MatrixMarket Format"};
    long_options[1] = {"iter",    required_argument, 0,  'i', "Iterations to be carried out in the case of coloring. In case of MPK the power value." };
    long_options[2] = {"cores",   required_argument, 0,  'c', "Number of cores to be used" };
    long_options[3] = {"smt",     required_argument, 0,  't', "Number of threads per core to be used (recommended 1)" };
    long_options[4] = {"cache_size",     required_argument, 0,  's', "Size of cache for blocking (in MB)" };
    long_options[5] = {"pin",     required_argument, 0,  'p', "Pinning strategy to be used; availablle options FILL or SCATTER" };
    long_options[6] = {"validate",no_argument,       0,  'v', "Validate coloring" };
    long_options[7] = {"tol",     required_argument, 0,  'T', "Tolerance for validation" };
    long_options[8] = {"convTol",     required_argument, 0,  'e', "Tolerance for convergence check" };
    long_options[9] = {"RCM",     no_argument, 0,  'R', "Do RCM pre-permutation" };
    long_options[10] = {"color_type",     no_argument, 0,  'C', "Coloring type: RACE (default), ABMC, MC" };
    long_options[11] = {"convergence_file",     required_argument, 0,  'f', "Output convergence history to a file. Specify file name. To be used with -v combination." };
    long_options[12] = {"help",    no_argument,       0,  'h', "Prints this help informations" };
    long_options[13] = {0,         0,                 0,   0,  0 };

    gnuOptions = new option[numOptions+1];

    for(int i=0; i<numOptions; ++i)
    {
        gnuOptions[i] = long_options[i].gnu_opt;
    }
}

parser::~parser()
{
    delete[] long_options;
    delete[] gnuOptions;
}

bool containsString(char *string, char *delim)
{
    std::string cpp_string(string);
    std::string cpp_delim(delim);
    if(cpp_string.find(cpp_delim) != std::string::npos)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool parser::parse_arg(int argc, char **argv)
{
    prgname = argv[0];
    while (1) {
        int option_index = 0, c;
        c = getopt_long(argc, argv, "0:m:i:c:t:n:s:p:T:e:C:f:vRh",
                gnuOptions, &option_index);

        if (c == -1)
            break;

        switch (c) {
            case 0:
                {
                    printf("No parameters specified.\n Usage: %s -m [MATRIX] -i [ITERATIONS] -c [CORES] -t [SMT] -n [numa domain] -s [cache size for blocking] -p [FILL/SCATTER]\n", prgname);
                    return false;
                    break;
                }
            case 'm':
                {
                    mat_file = optarg;
                    break;
                }
            case 'i':
                {
                    if(containsString(optarg, ","))
                    {
                        iter_vec = splitString<int>(optarg, ",");
                        iter = iter_vec[0]; //for backward compatibility
                    }
                    else
                    {
                        iter = atoi(optarg);
                        iter_vec = {iter};
                    }
                    break;
                }
            case 'c':
                {
                    printf("cores = %s\n", optarg);
                    cores = atoi(optarg);
                    break;
                }
            case 't':
                {
                    smt = atoi(optarg);
                    break;
                }
            case 's':
                {
                    if(containsString(optarg, ","))
                    {
                        cache_size_vec = splitString<double>(optarg, ",");
                        cache_size = cache_size_vec[0]; //for backward compatibility
                    }
                    else
                    {
                        cache_size = atof(optarg);
                        cache_size_vec = {cache_size};
                    }

                    break;
                }
            case 'p':
                {
                    char *pin_str = optarg;
                    printf("pin strategy = %s\n", pin_str);
                    if(!(strcmp(pin_str, "SCATTER")))
                    {
                        pin = SCATTER;
                    }
                    else if(!(strcmp(pin_str, "FILL")))
                    {
                        pin = FILL;
                    }
                    else
                    {
                        printf("%s pin strategy unknown. setting to FILL\n", pin_str);
                        pin = FILL;
                    }
                    break;
                }
            case 'v':
                {
                    validate = true;
                    break;
                }
            case 'T':
                {
                    tol = atof(optarg);
                    break;
                }
            case 'e':
                {
                    convTol = atof(optarg);
                    break;
                }
            case 'R':
                {
                    RCM_flag = true;
                    break;
                }
            case 'C':
                {
                    colorType = optarg;
                    break;
                }
            case 'f':
                {
                    convFile = optarg;
                    break;
                }
            case 'h':
                {
                    help();
                    return 0;
                    break;
                }
            default:
                break;
        }
    }

    if(mat_file==NULL)
    {
        printf("Please provide matrix file \n\n");
        help();
        return false;
    }
    else
    {
        return true;
    }
}

void parser::help()
{
    printf("Usage: %s [OPTION]...\n",prgname);
    printf("Valid options are:\n\n");
    char* HLINE = "────────────────────────────────────────────────────────────────────────────────────────────────────────────────";
    printf("%s\n",HLINE);
    printf("\t%s\t\t\t%s\n", "options", "description");
    printf("%s\n",HLINE);
    for(int i=0; i<numOptions; ++i)
    {
        char* long_opt;
        asprintf(&long_opt,"--%s", long_options[i].gnu_opt.name);
        printf("-%c or  %-12s |\t %-85s |\n", ((char) long_options[i].gnu_opt.val), long_opt, long_options[i].desc);
        free(long_opt);
    }

    printf("%s\n\n", HLINE);
    exit(0);
    /*    printf(" -m, --matrix=MATRIX FILE\t\tMatrix File in MatrixMarket Format\n");
          printf(" -c, --cores=CORES\t\tNumber of cores to be used\n");
          printf(" -t, --smt=THREADS PER CORE\t\tNumber of threads per core to be used (recommended 1)\n");
          printf(" -p, --pin=PIN STRATEGY\t\tPinning strategy to be used; availablle options FILL or SCATTER\n");
          printf(" -h, --help \t\t Prints this help informations\n");
          */
}
