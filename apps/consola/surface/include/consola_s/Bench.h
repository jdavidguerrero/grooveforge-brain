#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

namespace consola {
/** Fixed storage, loop-only diagnostics. No hardware access or allocation. */
class Bench {
public:
    enum class Command { Other, Scan, Inputs, WatchOff, WatchOn, InvalidScan, InvalidInputs, InvalidWatch };
    struct Snapshot {
        uint16_t mcp[4]{};
        int32_t enc[9]{}, ads[8]{}, fsr[4]{}, pot[3]{}, sel=0;
        uint16_t enc_mask=0;
        uint8_t mcp_valid=0, ads_valid=0, fsr_valid=0, pot_valid=0;
        bool sel_valid=false, m4=false;
        uint32_t m4_bits=0;
    };
    struct Event { const char* kind; uint8_t index; int32_t value; };
    /** Strict grammar for bench verbs; leaves existing commands untouched. */
    static Command parse(const char* line) {
        if (!strcmp(line,"@GF SCAN")) return Command::Scan;
        if (!strcmp(line,"@GF INPUTS")) return Command::Inputs;
        if (!strcmp(line,"@GF WATCH 0")) return Command::WatchOff;
        if (!strcmp(line,"@GF WATCH 1")) return Command::WatchOn;
        const char* verbs[]={"@GF SCAN","@GF INPUTS","@GF WATCH"};
        const Command errors[]={Command::InvalidScan,Command::InvalidInputs,Command::InvalidWatch};
        for (unsigned i=0;i<3;++i) {
            const size_t n=strlen(verbs[i]);
            if (!strncmp(line,verbs[i],n) && (line[n]==0 || line[n]==' ')) return errors[i];
        }
        return Command::Other;
    }
    /** Format without truncating a protocol line; false empties an undersized buffer. */
    static bool inputs(char* out, size_t size, const Snapshot& s, uint32_t drops) {
        Writer w(out,size);
        w.add("@GF OK INPUTS MCP=%04X,%04X,%04X,%04X MCP_VALID=%u ENC=",s.mcp[0],s.mcp[1],s.mcp[2],s.mcp[3],s.mcp_valid);
        w.list(s.enc,9); w.add(" ENC_MASK=%u ADS=",s.enc_mask); w.list(s.ads,8);
        w.add(" ADS_VALID=%u FSR=",s.ads_valid); w.list(s.fsr,4);
        w.add(" FSR_VALID=%u POT=",s.fsr_valid); w.list(s.pot,3);
        w.add(" POT_VALID=%u SEL=%ld SEL_VALID=%u M4=%u M4_BITS=%08lX DROPS=%lu\n",
              s.pot_valid,long(s.sel),unsigned(s.sel_valid),unsigned(s.m4),
              (unsigned long)s.m4_bits,(unsigned long)drops);
        return w.finish();
    }
    /** Begin a scan; addresses reserved by I2C are excluded. */
    void start_scan(uint32_t now) { scanning_=true; address_=8; found_count_=0; scan_since_=now-1000; }
    /** Return at most one due address; caller performs one bounded Wire transaction. */
    int scan_address(uint32_t now) {
        if (!scanning_ || uint32_t(now-scan_since_)<1000) return -1;
        scan_since_=now; return address_;
    }
    /** Record ACK and advance; returns true exactly on completion. */
    bool scan_result(bool ack) {
        if (!scanning_) return false;
        if (ack) found_[found_count_++]=address_;
        if (++address_==0x78) { scanning_=false; return true; }
        return false;
    }
    /** One-line final scan response, '-' means no ACKs. */
    bool scan_output(char* out,size_t size) const {
        Writer w(out,size); w.add("@GF OK SCAN ");
        if (!found_count_) w.add("-");
        for (uint8_t i=0;i<found_count_;++i) w.add("%s%02X",i?",":"",found_[i]);
        w.add("\n"); return w.finish();
    }
    /** True while a deferred SCAN response owns the command stream. */
    bool scanning() const { return scanning_; }
    /** Enable/disable watch and discard pending events, preserving cumulative drops. */
    void watch(bool enabled,uint32_t now,const Snapshot& s) {
        watching_=enabled; watch_since_=now; head_=tail_=count_=0; previous_=s;
    }
    /** Expire even if USB TX is stalled. Wrap-safe for the 120-second interval. */
    bool expire(uint32_t now) {
        if (!watching_ || uint32_t(now-watch_since_)<120000) return false;
        watching_=false; head_=tail_=count_=0; return true;
    }
    /** Queue a stable key edge or encoder position, dropping new events on overflow. */
    void event(const char* kind,uint8_t index,int32_t value) {
        if (!watching_) return;
        if (count_==64) { ++drops_; return; }
        events_[head_]={kind,index,value}; head_=(head_+1)%64; ++count_;
    }
    /** Publish raw analog changes above deadband; first valid sample is only a baseline. */
    void observe(const Snapshot& s) {
        if (!watching_) return;
        analog("ADS",s.ads,previous_.ads,8,s.ads_valid,previous_.ads_valid,128);
        analog("FSR",s.fsr,previous_.fsr,4,s.fsr_valid,previous_.fsr_valid,32);
        analog("POT",s.pot,previous_.pot,3,s.pot_valid,previous_.pot_valid,32);
        analog("SEL",&s.sel,&previous_.sel,1,s.sel_valid,previous_.sel_valid,32);
        previous_.ads_valid=s.ads_valid; previous_.fsr_valid=s.fsr_valid;
        previous_.pot_valid=s.pot_valid; previous_.sel_valid=s.sel_valid;
    }
    /** Dequeue one CDC event; false when empty. */
    bool next(char* out,size_t size) {
        if (!count_) return false;
        const auto& e=events_[tail_]; Writer w(out,size);
        w.add("@GF OK EVENT %s %u %ld\n",e.kind,e.index,long(e.value));
        if (!w.finish()) return false;
        tail_=(tail_+1)%64; --count_; return true;
    }
    /** Total lost watch events since boot; never silently reset by WATCH. */
    uint32_t drops() const { return drops_; }
private:
    class Writer {
    public:
        Writer(char* out,size_t size):out_(out),size_(size) { if(size)out[0]=0; }
        void add(const char* fmt,...) {
            if (!ok_) return;
            va_list args; va_start(args,fmt);
            const int n=vsnprintf(size_?out_+used_:nullptr,size_-used_,fmt,args); va_end(args);
            if (n<0 || size_t(n)>=size_-used_) ok_=false; else used_+=size_t(n);
        }
        void list(const int32_t* values,unsigned count) {
            for(unsigned i=0;i<count;++i) add("%s%ld",i?",":"",long(values[i]));
        }
        bool finish() { if(!ok_ && size_)out_[0]=0; return ok_; }
    private:
        char* out_; size_t size_,used_=0; bool ok_=true;
    };
    void analog(const char* kind,const int32_t* values,int32_t* old,unsigned n,
                uint8_t valid,uint8_t old_valid,int32_t threshold) {
        for(unsigned i=0;i<n;++i) {
            if (!(valid & (1u<<i))) continue;
            const int64_t delta=int64_t(values[i])-old[i];
            if (!(old_valid & (1u<<i))) old[i]=values[i];
            else if (delta>=threshold || delta<=-threshold) { event(kind,i,values[i]); old[i]=values[i]; }
        }
    }
    Snapshot previous_{};
    Event events_[64]{};
    uint8_t head_=0,tail_=0,count_=0,address_=8,found_[112]{},found_count_=0;
    uint32_t watch_since_=0,scan_since_=0,drops_=0;
    bool watching_=false,scanning_=false;
};
}
