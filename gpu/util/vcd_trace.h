#pragma once

#include <systemc.h>

class VcdTrace {
public:
    VcdTrace(const char *name, bool requireSuccess = true) {
        traceFile = sc_create_vcd_trace_file(name);
        if (requireSuccess && !traceFile) {
            throw 1; // todo use a nice macro
        }
    }

    ~VcdTrace() {
        if (traceFile) {
            sc_close_vcd_trace_file(traceFile);
        }
    }

    template <typename ObjectT>
    void trace(ObjectT &&object, const std::string &name) {
        if (!traceFile) {
            throw 1; // todo use a nice macro
        }
        sc_trace(traceFile, std::forward<ObjectT>(object), name);
    }

    template <typename ObjectT>
    void trace(ObjectT &&object) {
        if (!traceFile) {
            throw 1; // todo use a nice macro
        }
        sc_trace(traceFile, std::forward<ObjectT>(object), object.name());
    }

private:
    sc_trace_file *traceFile;
};

#define ADD_NAMED_TRACE(traceName, object) traceName.trace((object), #object)
#define ADD_TRACE(object) ADD_NAMED_TRACE(trace, (object))
