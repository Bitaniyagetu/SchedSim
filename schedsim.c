#include <stdio.h>
#include <stdlib.h>
#include "process.h"
#include "util.h"

// Round Robin gives each ready process up to one quantum per turn.
void findWaitingTimeRR(ProcessType plist[], int n, int quantum)
{
    if (n <= 0 || quantum <= 0)
        return;

    // Each process can appear in the queue at most once. The extra slot
    // lets head == tail represent an empty queue even when all n jobs are ready.
    size_t capacity = (size_t)n + 1;
    int *arrival_order = calloc((size_t)n, sizeof(*arrival_order));
    int *remaining = calloc((size_t)n, sizeof(*remaining));
    int *queue = calloc(capacity, sizeof(*queue));

    if (!arrival_order || !remaining || !queue) {
        free(arrival_order);
        free(remaining);
        free(queue);
        fprintf(stderr, "Error: Unable to allocate the Round Robin queue\n");
        exit(EXIT_FAILURE);
    }

    // Sort process indexes by arrival time and keep input order for ties.
    int count = 0;

    for (int i = 0; i < n; i++) {
        remaining[i] = plist[i].bt;
        plist[i].wt = 0;

        if (remaining[i] <= 0)
            continue;

        int position = count;
        while (position > 0 &&
               plist[arrival_order[position - 1]].art > plist[i].art) {
            arrival_order[position] = arrival_order[position - 1];
            position--;
        }

        arrival_order[position] = i;
        count++;
    }

    int time = 0;
    int next = 0;
    size_t head = 0;
    size_t tail = 0;

    while (next < count || head != tail) {
        // Advance to the next arrival if there is no ready process.
        if (head == tail && time < plist[arrival_order[next]].art)
            time = plist[arrival_order[next]].art;

        while (next < count && plist[arrival_order[next]].art <= time) {
            queue[tail] = arrival_order[next++];
            tail = (tail + 1) % capacity;
        }

        int process = queue[head];
        head = (head + 1) % capacity;

        int slice = remaining[process] < quantum
                        ? remaining[process] : quantum;
        time += slice;
        remaining[process] -= slice;

        // Admit new arrivals before requeuing the process that just ran.
        while (next < count && plist[arrival_order[next]].art <= time) {
            queue[tail] = arrival_order[next++];
            tail = (tail + 1) % capacity;
        }

        if (remaining[process] > 0) {
            queue[tail] = process;
            tail = (tail + 1) % capacity;
        } else {
            plist[process].wt = time - plist[process].art - plist[process].bt;
        }
    }

    free(queue);
    free(remaining);
    free(arrival_order);
}

// SRTF selects the ready process with the shortest remaining burst each tick.
void findWaitingTimeSJF(ProcessType plist[], int n)
{
    if (n <= 0)
        return;

    int *remaining = calloc((size_t)n, sizeof(*remaining));
    if (!remaining) {
        fprintf(stderr, "Error: Unable to allocate SJF burst times\n");
        exit(EXIT_FAILURE);
    }
    int unfinished = 0;
    int time = 0;

    for (int i = 0; i < n; i++) {
        remaining[i] = plist[i].bt;
        plist[i].wt = 0;

        if (remaining[i] > 0)
            unfinished++;
    }

    while (unfinished > 0) {
        int shortest = -1;

        for (int i = 0; i < n; i++) {
            if (plist[i].art <= time && remaining[i] > 0 &&
                (shortest == -1 || remaining[i] < remaining[shortest])) {
                shortest = i;
            }
        }

        if (shortest == -1) {
            time++;
            continue;
        }

        remaining[shortest]--;
        time++;

        if (remaining[shortest] == 0) {
            unfinished--;
            plist[shortest].wt =
                time - plist[shortest].art - plist[shortest].bt;
        }
    }
    free(remaining);
}

// Run processes in list order, accounting for idle time before an arrival.
void findWaitingTime(ProcessType plist[], int n)
{
    int time = 0;

    for (int i = 0; i < n; i++) {
        if (time < plist[i].art)
            time = plist[i].art;

        plist[i].wt = time - plist[i].art;
        time += plist[i].bt;
    }
}

void findTurnAroundTime(ProcessType plist[], int n)
{
    for (int i = 0; i < n; i++)
        plist[i].tat = plist[i].bt + plist[i].wt;
}

// Sort by descending priority, then by arrival time and input order.
int my_comparer(const void *this, const void *that)
{
    const ProcessType *first = this;
    const ProcessType *second = that;

    if (first->pri != second->pri)
        return (first->pri < second->pri) - (first->pri > second->pri);

    if (first->art != second->art)
        return (first->art > second->art) - (first->art < second->art);

    return (first->wt > second->wt) - (first->wt < second->wt);
}

void findavgTimeFCFS(ProcessType plist[], int n)
{
    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
    printf("\n*********\nFCFS\n");
}

void findavgTimeSJF(ProcessType plist[], int n)
{
    findWaitingTimeSJF(plist, n);
    findTurnAroundTime(plist, n);
    printf("\n*********\nSJF\n");
}

void findavgTimeRR(ProcessType plist[], int n, int quantum)
{
    findWaitingTimeRR(plist, n, quantum);
    findTurnAroundTime(plist, n);
    printf("\n*********\nRR Quantum = %d\n", quantum);
}

void findavgTimePriority(ProcessType plist[], int n)
{
    // Save input order in wt until the waiting times are calculated.
    for (int i = 0; i < n; i++)
        plist[i].wt = i;

    qsort(plist, n, sizeof(plist[0]), my_comparer);

    findWaitingTime(plist, n);
    findTurnAroundTime(plist, n);
    printf("\n*********\nPriority\n");
}

void printMetrics(ProcessType plist[], int n)
{
    int total_wt = 0;
    int total_tat = 0;

    printf("\tProcesses\tBurst time\tWaiting time\tTurn around time\n");

    for (int i = 0; i < n; i++) {
        total_wt += plist[i].wt;
        total_tat += plist[i].tat;
        printf("\t%d\t\t%d\t\t%d\t\t%d\n",
               plist[i].pid, plist[i].bt, plist[i].wt, plist[i].tat);
    }

    float awt = (float)total_wt / n;
    float att = (float)total_tat / n;

    printf("\nAverage waiting time = %.2f", awt);
    printf("\nAverage turn around time = %.2f\n", att);
}

ProcessType *initProc(char *filename, int *n)
{
    FILE *input_file = fopen(filename, "r");

    if (!input_file) {
        fprintf(stderr, "Error: Invalid filepath\n");
        exit(EXIT_FAILURE);
    }

    ProcessType *plist = parse_file(input_file, n);
    fclose(input_file);

    if (!plist || *n <= 0) {
        fprintf(stderr, "Error: No processes loaded from %s\n", filename);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < *n; i++) {
        if (plist[i].bt < 0 || plist[i].art < 0) {
            fprintf(stderr, "Error: Process %d has a negative burst or arrival time\n",
                    plist[i].pid);
            free(plist);
            exit(EXIT_FAILURE);
        }
    }

    return plist;
}

int main(int argc, char *argv[])
{
    int n = 0;
    int quantum = 2;
    ProcessType *proc_list;

    if (argc < 2) {
        fprintf(stderr, "Usage: ./schedsim <input-file-path>\n");
        return EXIT_FAILURE;
    }

    proc_list = initProc(argv[1], &n);
    findavgTimeFCFS(proc_list, n);
    printMetrics(proc_list, n);
    free(proc_list);

    n = 0;
    proc_list = initProc(argv[1], &n);
    findavgTimeSJF(proc_list, n);
    printMetrics(proc_list, n);
    free(proc_list);

    n = 0;
    proc_list = initProc(argv[1], &n);
    findavgTimePriority(proc_list, n);
    printMetrics(proc_list, n);
    free(proc_list);

    n = 0;
    proc_list = initProc(argv[1], &n);
    findavgTimeRR(proc_list, n, quantum);
    printMetrics(proc_list, n);
    free(proc_list);

    return EXIT_SUCCESS;
}