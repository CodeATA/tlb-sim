#include "pin.H"
#include <iostream>
#include <fstream>
#include <cstdio>

#include <vector>

using std::cerr;
using std::endl;
using std::string;
using std::vector;

typedef struct {
    UINT64 start_icount;
    UINT64 end_icount;
} region_info;

typedef struct {
    BOOL _active;
    INT _id;

    UINT64 _start;
    UINT64 _size;
    UINT64 _old_size;
} _range_info;

vector<_range_info> ranges;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; //number of dynamically executed instructions

region_info regions[100];
UINT32 cur_region = 0;
UINT32 total_region;

std::ofstream *out_trace = NULL;
std::ofstream *out_range = NULL;

FILE *mmap_trace = NULL;
FILE *id_trace = NULL;

string outNameBase;
string traceName, rangeName, mmapName;

BOOL in_mmap;
BOOL is_ranging = false;
BOOL is_tracing = false;

INT _remapping_idx;
// UINT64 _thresh = 64*1024;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutDir(KNOB_MODE_WRITEONCE, "pintool", "d", "", "specify output directory");

KNOB< string > KnobOutName(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify trace name");

KNOB< string > KnobWeight(KNOB_MODE_WRITEONCE, "pintool", "w", "", "weight file");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   numInstInBbl    number of instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID CountBbl(UINT32 numInstInBbl)
{
    char trace_suffix[500];
    char range_suffix[500];

    if (insCount >= regions[cur_region].end_icount)
    {
        is_tracing = false;
        is_ranging = true;
        cerr << "Region " << cur_region << " finished" << endl;
        out_trace->close();
        
        cur_region++;
        if (cur_region == total_region)
        {
            cerr << "Tracing finished" << endl;
            fclose(mmap_trace);
            fclose(id_trace);
            exit(0);
        }
        else
        {
            sprintf(trace_suffix, ".trace.%d", cur_region);
            sprintf(range_suffix, ".range.%d", cur_region);

            traceName = outNameBase + trace_suffix;
            rangeName = outNameBase + range_suffix;

            out_trace->open(traceName.c_str(), std::ios::binary|std::ios::out);
            if (!out_trace->is_open())
            {
                cerr << "Fail to open trace file " << traceName << endl;
                exit(-1);
            }
            out_range->open(rangeName.c_str(), std::ios::binary|std::ios::out);
            if (!out_range->is_open())
            {
                cerr << "Fail to open range file " << rangeName << endl;
                exit(-1);
            }
        }
    }
    else if (insCount >= regions[cur_region].start_icount)
    {
        is_ranging = false;
        is_tracing = true;
        if (out_range->is_open())
        {
            out_range->close();
            cerr << "Range " << cur_region << " finished" << endl;
        }
    }

    insCount += numInstInBbl;
    // cerr << "insCount: " << insCount << endl;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// Print a memory read record
VOID RecordMemRead(ADDRINT addr)
{
    if (in_mmap)
        return;

    // only record memory instructions in trace files
    if (!is_tracing)
        return;

    UINT8 opcode = 0;
    out_trace->write((char*)(&opcode), 1);
    out_trace->write((char*)(&addr), 8);
}

// Print a memory write record
VOID RecordMemWrite(ADDRINT addr)
{
    if (in_mmap)
        return;

    // only record memory instructions in trace files
    if (!is_tracing)
        return;

    UINT8 opcode = 1;
    out_trace->write((char*)(&opcode), 1);
    out_trace->write((char*)(&addr), 8);
}

VOID TraceCountIns(TRACE trace, VOID* v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl,
                    IARG_UINT32, BBL_NumIns(bbl),
                    IARG_END);

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            UINT32 memOpCount = INS_MemoryOperandCount(ins);
            for (UINT32 memOp = 0; memOp < memOpCount; memOp++)
            {
                if (INS_MemoryOperandIsRead(ins, memOp))
                {
                    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead, 
                                        IARG_MEMORYOP_EA, memOp,
                                        IARG_END);
                }
                if (INS_MemoryOperandIsWritten(ins, memOp))
                {
                    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite, 
                                        IARG_MEMORYOP_EA, memOp,
                                        IARG_END);
                }
            }
        }
    }
}

VOID catchArg_mmap(ADDRINT size)
{
    in_mmap = true;

    _range_info tmp_info;
    tmp_info._size = size;
    // range ID begins from 1 to simplify the simulator
    tmp_info._id = ranges.size() + 1;

    ranges.push_back(tmp_info);
}

VOID catchRet_mmap(ADDRINT ret)
{
    in_mmap = false;

    ranges.rbegin()->_active = true;
    ranges.rbegin()->_start = ret;

    UINT8 opcode = 2;
    INT id = ranges.rbegin()->_id;

    if (is_tracing)
    {
        // fprintf(stderr, "  record mmap in trace\n");
        out_trace->write((char*)(&opcode), 1);
        out_trace->write((char*)(&id), 4);
        out_trace->write((char*)(&ranges.rbegin()->_start), 8);
        out_trace->write((char*)(&ranges.rbegin()->_size), 8);
    }
    else if (is_ranging)
    {
        // fprintf(stderr, "  record mmap in range\n");
        out_range->write((char*)(&opcode), 1);
        out_range->write((char*)(&id), 4);
        out_range->write((char*)(&ranges.rbegin()->_start), 8);
        out_range->write((char*)(&ranges.rbegin()->_size), 8);
    }
    // *out_mmap << "+ " << id << " 0x" << std::hex << ranges.rbegin()->_start << " " << std::dec << ranges.rbegin()->_size << endl;
    fprintf(mmap_trace, "+ %d 0x%lx %lu\n", id, ranges.rbegin()->_start, ranges.rbegin()->_size);
    fprintf(id_trace, "+ %d %lu\n", id, ranges.rbegin()->_size);
}

VOID catchArg_mremap(ADDRINT old_address, ADDRINT old_size, ADDRINT new_size)
{
    // mremapCount++;
    in_mmap = true;

    for (unsigned i = 0; i < ranges.size(); i++)
    {
        if (!ranges[i]._active)
            continue;
        
        if ((old_address == ranges[i]._start) && (old_size == ranges[i]._size))
        {
            ranges[i]._size = new_size;
            ranges[i]._old_size = old_size;
            
            _remapping_idx = i;
            
            return;
        }
    }
}

VOID catchRet_mremap(ADDRINT ret)
{
    in_mmap = false;

    UINT8 opcode = 3;
    INT id = ranges[_remapping_idx]._id;
    if (is_tracing)
    {
        out_trace->write((char*)(&opcode), 1);
        out_trace->write((char*)(&id), 4);
        out_trace->write((char*)(&ranges[_remapping_idx]._start), 8);
        out_trace->write((char*)(&ranges[_remapping_idx]._old_size), 8);
        out_trace->write((char*)(&ret), 8);
        out_trace->write((char*)(&ranges[_remapping_idx]._size), 8);
    }
    else if (is_ranging)
    {
        out_range->write((char*)(&opcode), 1);
        out_range->write((char*)(&id), 4);
        out_range->write((char*)(&ranges[_remapping_idx]._start), 8);
        out_range->write((char*)(&ranges[_remapping_idx]._old_size), 8);
        out_range->write((char*)(&ret), 8);
        out_range->write((char*)(&ranges[_remapping_idx]._size), 8);
    }

    fprintf(mmap_trace, "= %d 0x%lx %lu 0x%lx %lu\n", id, ranges[_remapping_idx]._start, ranges[_remapping_idx]._old_size, ret, ranges[_remapping_idx]._size);
    fprintf(id_trace, "= %d %lu %lu\n", id, ranges[_remapping_idx]._old_size, ranges[_remapping_idx]._size);

    ranges[_remapping_idx]._start = ret;
}

VOID catchArg_munmap(ADDRINT addr, ADDRINT length)
{
    in_mmap = true;

    UINT8 opcode = 4;
    INT id = -1;
    for (unsigned i = 0; i < ranges.size(); i++)
    {
        if (!ranges[i]._active)
            continue;
        if (ranges[i]._start == addr)
        {
            assert(ranges[i]._size == length);

            id = ranges[i]._id;
            ranges[i]._active = false;
            break;
        }
    }
    assert(id != -1);

    if (is_tracing)
    {
        // cerr << "MUNMAP";
        out_trace->write((char*)(&opcode), 1);
        out_trace->write((char*)(&id), 4);
        out_trace->write((char*)(&addr), 8);
        out_trace->write((char*)(&length), 8);
    }
    else if (is_ranging)
    {
        out_range->write((char*)(&opcode), 1);
        out_range->write((char*)(&id), 4);
        out_range->write((char*)(&addr), 8);
        out_range->write((char*)(&length), 8);
    }
    // *out_mmap << "MUNMAP " << id << " 0x" << std::hex << addr << " " << std::dec << length << endl;
    fprintf(mmap_trace, "- %d 0x%lx %lu\n", id, addr, length);
    fprintf(id_trace, "- %d %lu\n", id, length);
}

VOID catchRet_munmap()
{
    in_mmap = false;
}

/*!
 * Pin calls this function every time a new rtn is executed
 * If the rountine is mmap(), we insert a call at its entry point to increment the count
 * @param[in]   rtn      routine to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID RoutineCatch_mmap(RTN rtn, VOID*v)
{
    if (RTN_Name(rtn).find("@plt") != string::npos)
        return;

    if (RTN_Name(rtn).find("mmap") != std::string::npos)
    {
        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)catchArg_mmap, 
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                    IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)catchRet_mmap,
                    IARG_FUNCRET_EXITPOINT_VALUE,
                    IARG_END);

        RTN_Close(rtn);
    }
    else if (RTN_Name(rtn).find("munmap") != std::string::npos)
    {
        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)catchArg_munmap, 
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                    IARG_END);
        
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)catchRet_munmap,
                    IARG_END);

        RTN_Close(rtn);
    }
    else if (RTN_Name(rtn).find("mremap") != std::string::npos)
    {
        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)catchArg_mremap, 
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                    IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)catchRet_mremap,
                    IARG_FUNCRET_EXITPOINT_VALUE,
                    IARG_END);

        RTN_Close(rtn);
    }
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    if (!KnobOutDir.Value().empty() && !KnobOutName.Value().empty())
    {
        outNameBase = KnobOutDir.Value() + '/' + KnobOutName.Value();

        traceName = outNameBase + ".trace.0";
        rangeName = outNameBase + ".range.0";

        out_trace = new std::ofstream(traceName.c_str(), std::ios::binary|std::ios::out);
        if (!out_trace->is_open())
        {
            cerr << "Fail to open trace file " << traceName << endl;
            return -1;
        }
        out_range = new std::ofstream(rangeName.c_str(), std::ios::binary|std::ios::out);
        if (!out_range->is_open())
        {
            cerr << "Fail to open range file " << rangeName << endl;
            return -1;
        }

        mmapName = outNameBase + ".mmap";
        mmap_trace = fopen(mmapName.c_str(), "w");
        if (mmap_trace == NULL)
        {
            cerr << "Fail to open mmap file " << mmapName << endl;
            return -1;
        }

        mmapName = outNameBase + ".id";
        id_trace = fopen(mmapName.c_str(), "w");
        if (id_trace == NULL)
        {
            cerr << "Fail to open mmap file (without address) " << mmapName << endl;
            return -1;
        }

        char *env_val = getenv("CAPAGING_MMAP_THRESHOLD");
        int __thresh = 65536;
        if (env_val != NULL)
        {
            sscanf(env_val, "%d", &__thresh);
        }

        mmapName = outNameBase + "._thresh";
        FILE *thresh_rec = fopen(mmapName.c_str(), "w");
        if (thresh_rec == NULL)
        {
            cerr << "Fail to open mmap file (without address) " << mmapName << endl;
            return -1;
        }
        fprintf(thresh_rec, "mallopt parameter (>=): %d\n", __thresh);
        fclose(thresh_rec);
    }
    else 
    {
        cerr << "ERROR: No output info" << endl;
        return Usage();
    }

    if (KnobWeight.Value().empty())
    {
        cerr << "ERROR: No weight file" << endl;
        return Usage();
    }
    
    std::ifstream *weights_file = new std::ifstream(KnobWeight.Value().c_str(), std::ios::in);
    if (!weights_file->is_open())
    {
        cerr << "ERROR: fail to open weight file" << endl;
        cerr << "  " << KnobWeight.Value() << endl;
        return 0;
    }
    char line[200];
    weights_file->getline(line, 200);
    sscanf(line, "%u", &total_region);
    fprintf(stderr, "Total region: %u\n", total_region);
    for (unsigned i = 0; i < total_region; i++)
    {
        weights_file->getline(line, 200);
        sscanf(line, "%lu,%lu", &regions[i].start_icount, &regions[i].end_icount);
    }
    weights_file->close();
    delete weights_file;

    in_mmap = false;
    cur_region = 0;
    is_ranging = true;
    PIN_InitSymbols();

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(TraceCountIns, 0);

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(RoutineCatch_mmap, 0);

    // Register function to be called when the application exits
    // PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
