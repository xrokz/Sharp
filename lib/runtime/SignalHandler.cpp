//
// Created by BraxtonN on 6/29/2018.
//

#include <csignal>
#include <iostream>
#include <cstdlib>

using namespace std;

namespace
{
    volatile std::sig_atomic_t gSignalStatus;
}

extern void printRegs();

string signalToString(int signal) {
    switch(signal) {
        case SIGINT:
            return "SIGINT";
        case SIGSEGV:
            return "SIGSEGV";
        case SIGILL:
            return "SIGILL";
        case SIGTERM:
            return "SIGTERM";
        case SIGABRT:
            return "SIGABRT";
    }
    return "?";
}

void os_signal(int signal)
{
    gSignalStatus = signal;

    if(signal != SIGINT) {
        cerr << "(" << signalToString(signal) << ") found, please contact the language developer with the call stack below" << endl;
        printRegs();
    }
    exit(1);
}

void setupSigHandler() {
    std::signal(SIGINT, os_signal);
    std::signal(SIGSEGV, os_signal);
    std::signal(SIGILL, os_signal);
    std::signal(SIGTERM, os_signal);
    std::signal(SIGABRT, os_signal);
}