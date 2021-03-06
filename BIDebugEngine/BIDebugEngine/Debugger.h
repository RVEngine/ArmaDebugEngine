#pragma once
#include <map>
#include <memory>
#include "RVClasses.h"
#include "BreakPoint.h"
#include "Monitor.h"
#include "NetworkController.h"
#include <shared_mutex>

namespace std {
    class mutex;
    class condition_variable;
}

struct RV_VMContext;
class Script;
class VMContext;

struct DebuggerInstructionInfo {
    RV_GameInstruction* instruction;
    RV_VMContext* context;
    GameState* gs;
};

extern scriptExecutionContext currentContext;
extern MissionEventType currentEventHandler;

struct BreakStateInfo {
    const DebuggerInstructionInfo* instruction{ nullptr };
    BreakPoint* bp{ nullptr };
};

enum class DebuggerState {
    Uninitialized,
    running,
    breakState,
    stepState
};

enum class VariableScope { //This is a bitflag
    invalid = 0,
    callstack = 1,
    local = 2,
    missionNamespace = 4,
    uiNamespace = 8,
    profileNamespace = 16,
    parsingNamespace = 32
};
inline VariableScope operator | (VariableScope lhs, VariableScope rhs) {
    return static_cast<VariableScope>(static_cast<std::underlying_type_t<VariableScope>>(lhs) | static_cast<std::underlying_type_t<VariableScope>>(rhs));
}

inline std::underlying_type_t<VariableScope> operator & (VariableScope lhs, VariableScope rhs) {
    return (static_cast<std::underlying_type_t<VariableScope>>(lhs) & static_cast<std::underlying_type_t<VariableScope>>(rhs));
}

struct HookIntegrity {
    bool __scriptVMConstructor{ false };
    bool __scriptVMSimulateStart{ false };
    bool __scriptVMSimulateEnd{ false };
    bool __instructionBreakpoint{ false };
    bool __worldSimulate{ false };
    bool __worldMissionEventStart{ false };
    bool __worldMissionEventEnd{ false };
    bool __onScriptError{ false };
    bool scriptPreprocDefine{ false };
    bool scriptPreprocConstr{ false };
    bool scriptAssert{ false };
    bool scriptHalt{ false };
    bool scriptEcho{ false };
    bool engineAlive{ false };
    bool enableMouse{ false };
};



class Debugger {
public:
    Debugger();
    ~Debugger();

    //#TODO Variable read/write breakpoints GameInstructionVariable/GameInstructionAssignment
    void clear();
    std::shared_ptr<VMContext> getVMContext(RV_VMContext* vm);
    void writeFrameToFile(uint32_t frameCounter);
    void onInstruction(DebuggerInstructionInfo& instructionInfo);
    void onScriptError(GameState* gs);
    void onScriptAssert(GameState* gs);
    void onScriptHalt(GameState* gs);
    void checkForBreakpoint(DebuggerInstructionInfo& instructionInfo);
    void onShutdown();
    void onStartup();

    void onHalt(std::shared_ptr<std::pair<std::condition_variable, bool>> waitEvent, BreakPoint* bp, const DebuggerInstructionInfo& info, haltType type); //Breakpoint is halting engine
    void onContinue(); //Breakpoint has stopped halting
    void commandContinue(StepType stepType); //Tells Breakpoint in breakState to Stop halting
    void setGameVersion(const char* productType, const char* productVersion);
    void SerializeHookIntegrity(JsonArchive& answer);
    void onScriptEcho(RString msg);
    void serializeScriptCommands(JsonArchive& answer);
    HookIntegrity HI;
    GameState* lastKnownGameState;

    void setHookIntegrity(HookIntegrity hi) { HI = hi; }

    struct VariableInfo {
        VariableInfo() {}
        VariableInfo(const GameVariable* _var, VariableScope _ns) : var(_var), ns(_ns) {};
        VariableInfo(RString _name) : var(nullptr), ns(VariableScope::invalid), notFoundName(_name) {};
        const GameVariable* var;
        VariableScope ns;
        RString notFoundName;
        void Serialize(JsonArchive& ar) const;
    };
    std::vector<VariableInfo> getVariables(VariableScope, std::vector<std::string>& varName) const;
    void grabCurrentCode(JsonArchive& answer,const std::string& file) const;
    std::map<uintptr_t, std::shared_ptr<VMContext>> VMPtrToScript;
    //std::map<std::string, std::vector<BreakPoint>> breakPoints;
    class breakPointList : public std::vector<BreakPoint> {
    public:
        breakPointList() {}
        // breakPointList(const breakPointList& b) = delete;// : _name(b._name) { for (auto &it : b) emplace_back(std::move(it)); } //std::vector like's to still copy while passing rvalue-ref
         //breakPointList(const BreakPoint& b) : _name(b.filename) {push_back(b); }
        breakPointList(BreakPoint&& b) noexcept : _name(b.filename) { push_back(std::move(b)); }
        breakPointList& operator=(breakPointList&& b) noexcept {
            _name = (b._name); for (auto &it : b) emplace_back(std::move(it));
            return *this;
        }
        RString _name;
        const char* getMapKey()const { return _name; }
        breakPointList(const breakPointList& b) { if (!b._name.isNull()) __debugbreak(); }
    private:

        // disable copying

        breakPointList& operator=(const breakPointList&);


    };
    std::shared_mutex breakPointsLock;
    MapStringToClassNonRV<breakPointList, std::vector<breakPointList>> breakPoints; //All inputs have to be tolowered before accessing
    std::vector<std::shared_ptr<IMonitorBase>> monitors;
    NetworkController nController;
    std::shared_ptr<std::pair<std::condition_variable, bool>> breakStateContinueEvent;
    DebuggerState state{ DebuggerState::Uninitialized };
    BreakStateInfo breakStateInfo;
    struct {
        StepType stepType;
        uint16_t stepLine;
        uint8_t stepLevel;
        RV_VMContext* context;
    } stepInfo;
    struct productInfoStruct {
        RString gameType;
        RString gameVersion;
        void Serialize(JsonArchive &ar);
    } productInfo;
};

