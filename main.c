#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
#include "util/debug.h"
#include "util/files.h"
#include "util/print.h"
#include "util/defs.h"
#include "mcc/mcc.h"
#include "mcc/cms_tree.h"
#include "tasa/tasa.h"
#include "modesa/modesa.h"
#include "schedule/schedule.h"
#include "schedule/no-schedule.h"
#include "schedule/fhss.h"
#include "rpl/rpl.h"

#define EXECUTE_SCHEDULE        0           /* This is 1 if we are going to simulate the schedule */
#define EXECUTE_RPL             1
#define EXECUTE_FLOODING        0

#define TSCH_PROTOCOL           NO_SCHEDULE
#define RPL_PROTOCOL            RPL_MRHOF
#define SINK_NODE               0
#define SENSOR_NODE             39
#define CHANNEL                 15          /* Channel to be considered for single-channel algorithms */
#define EXPORT_MASK_CHANNELS    0           /* This is 1 if we are going to output a mask with all channels that could be used */
#define FHSS                    FHSS_OPENWSN    /* FHSS_OPENWSN, FHSS_DISTRIBUTED_BLACKLIST_OPTIMAL FHSS_DISTRIBUTED_BLACKLIST_MAB_BEST_ARM */
#define PKT_PROB                1
#define ETX_THRESHOLD           0.5
#define DUTY_CYCLE              0.77
#define SLOTFRAME_SIZE          13
#define PROB_TX                 3
#define LOG_TYPE                32          /* All 6 logs */

#define DATA_FILE "data/prr_tutornet/rpl-tamu/prr40_1.dat"
#define LINKS_PREFIX "data/prr_tutornet/rpl-tamu/prr40"
#define TREE_FILE "tree.dat"

//#define N_TIMESLOTS_PER_FILE    23400       // 15 minutes per file and 39 time slots per 1.5 second (900 s x 39 ts / 1.5 s = 23400 ts per file)
#define N_TIMESLOTS_PER_FILE    9000
//#define N_TIMESLOTS_LOG         1560        // log every 1 minute (60 s x 39 ts / 1.5 s = 1560 ts per minute)
#define N_TIMESLOTS_LOG         1000        // log every 30 seconds
#define MAX_N_FILES             100

void readPrrFile(char *file_name, List *nodesList, List linksList[]);
void initializeTree(uint8_t alg, Tree_t **tree, List *nodesList, uint8_t sink_id, bool conMatrix[][MAX_NODES][NUM_CHANNELS], List linksList[], int8_t channel);
void createMatrices(List *nodesList, List linksList[], float prrMatrix[][MAX_NODES][NUM_CHANNELS], bool conMatrix[][MAX_NODES][NUM_CHANNELS], \
                    bool intMatrix[][MAX_NODES][NUM_CHANNELS], bool confMatrix[][MAX_NODES][NUM_CHANNELS]);

void printHelp(void)
{
    printf("HELP:\n");
    printf("./Scheduling <alg> <sink_id> <channel> <export_mask_channels> <ext_threshold> <file_name> <execute_sch> <links_prefix> <fhss>:\n");
    printf("<tsch_alg>: 0 - MCC_ICRA; 1 - MCC_CQAA; 2 - MCC_CQARA; 3 - TASA; \n\t4 - MODESA; 5 - MCC_ICRA_NONOPTIMAL; 6 - NO_SCHEDULE\n");
    printf("<sink_id>: sink node\n");
    printf("<channel>: 0 to 15\n");
    printf("<export_mask_channels>: 0 or 1\n");
    printf("<etx_threshold>: 0.0 to 1.0\n");
    printf("<file_name>: file name (including extension)\n");
    printf("<links_prefix>: prefix of file names with link information\n");
    printf("<fhss>: 0 - 10 \n");
    printf("<n_timeslots_per_file>: number of timeslots per file \n");
    printf("<n_timeslots_log>: number of timeslots per line of the log files \n");
    printf("<log_type>: bitmap with log types to output \n");
    printf("<execute_sch>: 0 or 1\n");
    printf("if execute_sch == 1\n");
    printf("- <pkt_prob>: 0 - 100\n");
    printf("if execute_sch == 0\n");
    printf("- <execute_rpl>: 0 or 1\n");
    printf("-- if execute_rpl == 1\n");
    printf("--- <rpl_alg>: 1 - RPL_MRHOF; 2 - RPL_TAMU_MULTIHOP_RANK; 5 - RPL_WITH_DIJKSTRA\n");
    printf("--- <rank_interval>: interval in timeslots to calculate the rank on RPL\n");
    printf("--- <default_link_cost>: default RPL link cost (default ETX)\n");
    printf("- <execute_flooding>: 0 or 1\n");
    printf("-- if execute_flooding == 1\n");
    printf("--- <sensor_id>: sensor node\n");
    printf("--- <slotframe_size>: number of time slots in a slotframe\n");
    printf("--- <duty_cycle>: duty-cycle\n");
    printf("--- <prob_tx>: probability of TX packet\n");
}

int main(int argc, char *argv[])
{
    uint8_t sink_id, tsch_alg, channel, fhss, pkt_prob, rpl_alg, log_type;
    bool execute_sch, export_mask_channels, execute_rpl, execute_flooding;
    uint32_t n_timeslots_per_file, n_timeslots_log;
    float etx_threshold;
    uint8_t slotframe_size, prob_tx, sensor_id;
    float duty_cycle;
    char file_name[50];
    char links_prefix[50];

    /* Checking if we have user defined parameters */
    if (argc > 1)
    {
        if (strcmp(argv[1],"-h") == 0)
        {
            printHelp();
            return 0;
        }

        uint8_t arg_i = 1;
        tsch_alg = atoi(argv[arg_i++]);
        sink_id = atoi(argv[arg_i++]);
        channel = atoi(argv[arg_i++]);
        export_mask_channels = atoi(argv[arg_i++]);
        etx_threshold = atof(argv[arg_i++]);
        strcpy(file_name,argv[arg_i++]);
        strcpy(links_prefix,argv[arg_i++]);
        fhss = atoi(argv[arg_i++]);
        n_timeslots_per_file = atoi(argv[arg_i++]);
        n_timeslots_log = atoi(argv[arg_i++]);
        log_type = atoi(argv[arg_i++]);
        execute_sch = atoi(argv[arg_i++]);
        if (execute_sch)
        {
            pkt_prob = atoi(argv[arg_i++]);
            if (fhss == FHSS_CENTRALIZED_BLACKLIST)
            {
                schedulSetBlacklistSize(atoi(argv[arg_i++]));
            }
            else if (fhss == FHSS_DISTRIBUTED_BLACKLIST_MAB_FIRST_BEST_ARM || \
                     fhss == FHSS_DISTRIBUTED_BLACKLIST_MAB_FIRST_GOOD_ARM || \
                     fhss == FHSS_DISTRIBUTED_BLACKLIST_MAB_BEST_ARM)
            {
                fhssSetEpsilonN(atoi(argv[arg_i++]));
                fhssSetEpsilonInitN(atoi(argv[arg_i++]));
                fhssSetEpsilonTSIncrN(atoi(argv[arg_i++]));
                fhssSetEpsilonMaxN(atoi(argv[arg_i++]));

                if (fhss == FHSS_DISTRIBUTED_BLACKLIST_MAB_FIRST_BEST_ARM)
                {
                    fhssSetMABFirstBestArms(atoi(argv[arg_i++]));
                }
                else if (fhss == FHSS_DISTRIBUTED_BLACKLIST_MAB_FIRST_GOOD_ARM)
                {
                    fhssSetMABThreshooldGoodArm(atoi(argv[arg_i++]));
                }
            }
        }
        else
        {
            execute_rpl = atoi(argv[arg_i++]);
            if (execute_rpl)
            {
                rpl_alg = atoi(argv[arg_i++]);
                rplSetRankInterval(atoi(argv[arg_i++]));
                rplSetDefaultLinkCost(atoi(argv[arg_i++]));
            }
            execute_flooding = atoi(argv[arg_i++]);
            if (execute_flooding)
            {
                sensor_id = atoi(argv[arg_i++]);
                slotframe_size = atoi(argv[arg_i++]);
                duty_cycle = atof(argv[arg_i++]);
                prob_tx = atoi(argv[arg_i++]);
            }
        }
    }
    /* Use hard-coded parameters */
    else
    {
        tsch_alg = TSCH_PROTOCOL;
        sink_id = SINK_NODE;
        channel = CHANNEL;
        export_mask_channels = EXPORT_MASK_CHANNELS;
        etx_threshold = ETX_THRESHOLD;
        strcpy(file_name, DATA_FILE);
        strcpy(links_prefix, LINKS_PREFIX);
        fhss = FHSS;
        n_timeslots_per_file = N_TIMESLOTS_PER_FILE;
        n_timeslots_log = N_TIMESLOTS_LOG;
        execute_sch = EXECUTE_SCHEDULE;
        execute_rpl = EXECUTE_RPL;
        execute_flooding = EXECUTE_FLOODING;
        pkt_prob = PKT_PROB;
        rpl_alg = RPL_PROTOCOL;
        sensor_id = SENSOR_NODE;
        slotframe_size = SLOTFRAME_SIZE;
        duty_cycle = DUTY_CYCLE;
        prob_tx = PROB_TX;
        log_type = LOG_TYPE;
    }

    /* Initializing the RGN */
    time_t t;
    srand((unsigned) time(&t));

    /* List of nodes */
    List nodesList; memset(&nodesList, 0, sizeof(List)); ListInit(&nodesList);

    /* List of links */
    List linksList[NUM_CHANNELS];
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
    {
        memset(&linksList[i], 0, sizeof(List)); ListInit(&linksList[i]);
    }

    /* Read input from file with PRR values*/
    readPrrFile(file_name, &nodesList, linksList);

    /* Create the ETX matrix */
    float etxMatrix[MAX_NODES][MAX_NODES][NUM_CHANNELS];
    memset(etxMatrix, 0, MAX_NODES * MAX_NODES * NUM_CHANNELS * sizeof(float));
    createEtxMatrix(etxMatrix, linksList);

    /* Create the connectivity matrix considering any PPR */
    bool conMatrix[MAX_NODES][MAX_NODES][NUM_CHANNELS];
    memset(conMatrix, false, MAX_NODES * MAX_NODES * NUM_CHANNELS * sizeof(bool));
    createConnectivityMatrix(conMatrix, linksList, 1.0/etx_threshold);

    /* Create the interference matrix considering any interference threshold */
    bool intMatrix[MAX_NODES][MAX_NODES][NUM_CHANNELS];
    memset(intMatrix, false, MAX_NODES * MAX_NODES * NUM_CHANNELS * sizeof(bool));
    createInterferenceMatrix(intMatrix, linksList, 1.0);

    /* Create the conflict matrix considering the connectivity, interference and cmsTree */
    bool confMatrix[MAX_NODES][MAX_NODES][NUM_CHANNELS];
    memset(confMatrix, false, MAX_NODES * MAX_NODES * NUM_CHANNELS * sizeof(bool));
    createConflictMatrix(NULL, intMatrix, &nodesList, confMatrix, false);

    /* Create the distribution tree */
    Tree_t *tree; initializeTree(tsch_alg, &tree, &nodesList, sink_id, conMatrix, linksList, channel);

    /* Print network parameters */
    printNetworkParameters(tree, linksList, &nodesList, conMatrix, intMatrix, confMatrix, etxMatrix);

    /* Lets choose which protocol we want to work with */
    if (tsch_alg == MCC_ICRA)
    {
        main_mcc(&nodesList, &linksList[channel], tree, sink_id, intMatrix, confMatrix, NULL, false, false, true, channel, etx_threshold);
    }
    else if (tsch_alg == MCC_ICRA_NONOPTIMAL)
    {
        main_mcc(&nodesList, &linksList[channel], tree, sink_id, intMatrix, confMatrix, NULL, false, false, false, channel, etx_threshold);
    }
    else if (tsch_alg == MCC_CQAA)
    {
        main_mcc(&nodesList, &linksList[channel], tree, sink_id, intMatrix, confMatrix, etxMatrix, false, true, false, -1, etx_threshold);
    }
    else if (tsch_alg == MCC_CQARA)
    {
        main_mcc(&nodesList, &linksList[channel], tree, sink_id, intMatrix, confMatrix, etxMatrix, true, true, false, -1, etx_threshold);
    }
    else if (tsch_alg == TASA)
    {
        main_tasa(&nodesList, &linksList[channel], tree, sink_id, intMatrix, confMatrix, channel);
    }
    else if (tsch_alg == MODESA)
    {
        main_modesa(&nodesList, &linksList[channel], tree, sink_id, 1, intMatrix, confMatrix, channel);
    }
    else if (tsch_alg == NO_SCHEDULE)
    {
        main_no_schedule(&nodesList, slotframe_size, 1, duty_cycle);
    }
    else
    {
        EXIT("Invalid TSCH algorithm %d\n", tsch_alg);
    }

    /* Print network parameters */
    printNetworkParameters(tree, linksList, &nodesList, conMatrix, intMatrix, confMatrix, etxMatrix);

    /* Execute the schedule */
    if (execute_sch)
    {
        /* Initializing the RGN */
        time_t t;
        srand((unsigned) time(&t));

        /* Generate 'n_timeslots_per_file' random numbers to be used in the recpetion decision */
        List draws; memset(&draws, 0, sizeof(List)); ListInit(&draws);
        for (uint64_t i = 0; i < n_timeslots_per_file*MAX_N_FILES; i++)
        {
            uint8_t sample = rand() % 100;
            ListAppend(&draws, (void *)sample);
        }

        /* Run each type of FHSS algorithm */
        if (fhss != FHSS_ALL)
        {
            run_schedule(fhss, &draws, &nodesList, tree, sink_id, links_prefix, n_timeslots_per_file, n_timeslots_log, pkt_prob);
        }
        else
        {
            for (uint8_t i = 0; i < FHSS_ALL; i++)
            {
                run_schedule(i, &draws, &nodesList, tree, sink_id, links_prefix, n_timeslots_per_file, n_timeslots_log, pkt_prob);
            }
        }
    }
    else
    {
        /* Execute the RPL for routing tree calculation */
        if (execute_rpl)
        {
            run_rpl(rpl_alg, &nodesList, tree, sink_id, channel, links_prefix, n_timeslots_per_file, N_TIMESLOTS_PER_DIO, N_TIMESLOTS_PER_KA, N_TIMESLOTS_PER_DATA, n_timeslots_log, log_type);
        }

        /* Execute the Flooding protocol with alarm application */
        if (execute_flooding)
        {
            run_no_schedule(sink_id, sensor_id, 850, prob_tx, &nodesList, links_prefix, n_timeslots_per_file, n_timeslots_log);
        }
    }

    if (tsch_alg != NO_SCHEDULE)
    {
        /* Write output to files */
        output(tsch_alg, &nodesList, tree, TREE_FILE, export_mask_channels, false);
    }

    return (0);
}

void readPrrFile(char *file_name, List *nodesList, List linksList[])
{
    /* Input file */
    FILE *fp = NULL;

    /* Opening file */
    int res = openFile(&fp, file_name, "r");

    if (res != true)
    {
        EXIT("Error while reading file %s", file_name);
    }

    /* Read the input file and insert links into the list */
    PRINTF("Reading the file with number of nodes and links!\n");
    readFile(fp, nodesList, linksList, NULL, 100);

    fclose(fp);
}

void initializeTree(uint8_t alg, Tree_t **tree, List *nodesList, uint8_t sink_id, bool conMatrix[][MAX_NODES][NUM_CHANNELS], List linksList[], int8_t channel)
{
    uint8_t res = 0;

    /* Input file */
    FILE *fp = NULL;

    if ((alg == TASA) || (alg == MODESA))
    {
        /* Opening file */
        res = openFile(&fp, TREE_FILE, "r");
        if (res != true) EXIT("Error while reading file %s", TREE_FILE);

        /* Create the connected tree */
        if (alg == TASA)
        {
            *tree = newTree(getNode(sink_id, nodesList), TASA_TREE);
        }
        else if (alg == MODESA)
        {
            *tree = newTree(getNode(sink_id, nodesList), MODESA_TREE);
        }

        if (!readFile(fp, nodesList, NULL, *tree, 100))
        {
            EXIT("It is impossible to create the tree from the given file.\n");
        }
        fclose(fp);
    }
    else if (alg == MCC_ICRA || alg == MCC_ICRA_NONOPTIMAL || alg == MCC_CQAA)
    {
        /* Create the Capacitated Minimum Spanning Tree */
        Node_t *sink = getNode(sink_id, nodesList);
        sink->type = SINK;
        *tree = constructCMSTreeSingleChannel(sink, nodesList, conMatrix, channel);
    }
    else if (alg == MCC_CQARA)
    {
        /* Create the Capacitated Minimum Spanning Tree with multiple channels */
        Node_t *sink = getNode(sink_id, nodesList);
        sink->type = SINK;
        *tree = constructCMSTreeMultipleChannel(sink, nodesList, conMatrix, linksList);
    }
    else if (alg == NO_SCHEDULE)
    {
        // Lets just initialize the tree, we dont need to create it
        *tree = newTree(getNode(sink_id, nodesList), NO_SCHEDULE);
    }

    /* Set the type of each node properly */
    setTypeOfNodes(sink_id, *tree);
}



