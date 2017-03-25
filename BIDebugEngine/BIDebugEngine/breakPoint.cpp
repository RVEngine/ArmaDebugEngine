#include "BreakPoint.h"
#include "Debugger.h"
#include "Serialize.h"
#include <condition_variable>
#include <memory>
#include <fstream>
#include <chrono>
using namespace std::chrono_literals;

BreakPoint::BreakPoint(uint16_t _line) :line(_line) {}


BreakPoint::~BreakPoint() {}

void BreakPoint::Serialize(JsonArchive& ar) {
    ar.Serialize("filename", filename);
    ar.Serialize("line", line);

    if (!ar.reading()) {
        ar.Serialize("condition", condition);
        ar.Serialize("action", action);
    } else {
        auto pJson = ar.getRaw();

        if (!(*pJson)["condition"].is_null()) {
            auto condJson = (*pJson)["condition"];
            auto type = static_cast<BPCondition_types>(condJson.value<int>("type", 0));
            switch (type) {

                case BPCondition_types::invalid: break;
                case BPCondition_types::Code: {
                    condition = std::make_unique<BPCondition_Code>();
                    JsonArchive condJsonAr(condJson);
                    condition->Serialize(condJsonAr);
                } break;
                default: break;
            }
        }
        if (!(*pJson)["action"].is_null()) {
            auto actJson = (*pJson)["action"];
            auto type = static_cast<BPAction_types>(actJson.value<int>("type", 0));
            switch (type) {

                case BPAction_types::invalid: break;
                case BPAction_types::ExecCode: {
                    action = std::make_unique<BPAction_ExecCode>();
                    JsonArchive actJsonAr(actJson);
                    action->Serialize(actJsonAr);
                } break;
                default: break;
                case BPAction_types::Halt: {
                    action = std::make_unique<BPAction_Halt>(haltType::breakpoint);
                    JsonArchive actJsonAr(actJson);
                    action->Serialize(actJsonAr);
                } break;
                case BPAction_types::LogCallstack: {
                    action = std::make_unique<BPAction_LogCallstack>();
                    JsonArchive actJsonAr(actJson);
                    action->Serialize(actJsonAr);
                } break;
            }
        }
        label = pJson->value("label", "");
    }





}

bool BreakPoint::trigger(Debugger* dbg, const DebuggerInstructionInfo& instructionInfo) {
    if (condition && !condition->isMatching(dbg, this, instructionInfo)) return false;
    hitcount++;



    /*
    JsonArchive varArchive;
    JsonArchive nsArchive[4];
    auto func = [](JsonArchive& nsArchive, const GameDataNamespace* var) {
    var->_variables.forEach([&nsArchive](const GameVariable& var) {

    JsonArchive variableArchive;

    auto name = var._name;
    if (var._value.isNull()) {
    variableArchive.Serialize("type", "nil");
    nsArchive.Serialize(name.data(), variableArchive);
    return;
    }
    auto value = var._value._data;
    const auto type = value->getTypeString();

    variableArchive.Serialize("type", type);
    if (strcmp(type, "array") == 0) {
    variableArchive.Serialize("value", value->getArray());
    } else {
    variableArchive.Serialize("value", value->getAsString());
    }
    nsArchive.Serialize(name.data(), variableArchive);

    });
    };

    std::thread _1([&]() {func(nsArchive[0], instructionInfo.gs->_namespaces.get(0)); });
    std::thread _2([&]() {func(nsArchive[1], instructionInfo.gs->_namespaces.get(1)); });
    std::thread _3([&]() {func(nsArchive[2], instructionInfo.gs->_namespaces.get(2)); });
    std::thread _4([&]() {func(nsArchive[3], instructionInfo.gs->_namespaces.get(3)); });


    if (_1.joinable()) _1.join();
    varArchive.Serialize(instructionInfo.gs->_namespaces.get(0)->_name.data(), nsArchive[0]);
    if (_2.joinable()) _2.join();
    varArchive.Serialize(instructionInfo.gs->_namespaces.get(1)->_name.data(), nsArchive[1]);
    if (_3.joinable()) _3.join();
    varArchive.Serialize(instructionInfo.gs->_namespaces.get(2)->_name.data(), nsArchive[2]);
    if (_4.joinable()) _4.join();
    varArchive.Serialize(instructionInfo.gs->_namespaces.get(3)->_name.data(), nsArchive[3]);

    auto text = varArchive.to_string();
    std::ofstream f("P:\\AllVars.json", std::ios::out | std::ios::binary);
    f.write(text.c_str(), text.length());
    f.close();



    */




    executeActions(dbg, instructionInfo);

    //JsonArchive ar;
    //instructionInfo.context->Serialize(ar);
    //auto text = ar.to_string();
    //std::ofstream f("P:\\break.json", std::ios::out | std::ios::binary);
    //f.write(text.c_str(), text.length());
    //f.close();
    //std::ofstream f2("P:\\breakScript.json", std::ios::out | std::ios::binary);
    //f2.write(instructionInfo.instruction->_scriptPos._content.data(), instructionInfo.instruction->_scriptPos._content.length());
    //f2.close();
    return true;
}

void BreakPoint::executeActions(Debugger* dbg, const DebuggerInstructionInfo& instructionInfo) {
    if (action) action->execute(dbg, this, instructionInfo);
}

bool BPCondition_Code::isMatching(Debugger*, BreakPoint*, const DebuggerInstructionInfo& info) {
    auto rtn = info.context->callStack.back()->EvaluateExpression(code.c_str(), 10);
    if (rtn.isNull())  return false; //#TODO this is code error.
    //We get a ptr to the IDebugValue of GameData. But we wan't the GameData vtable.    
    auto gdRtn = reinterpret_cast<GameData*>(rtn.get() - 2);
    return gdRtn->getBool();
}

void BPCondition_Code::Serialize(JsonArchive& ar) {
    if (!ar.reading()) {
        ar.Serialize("type", static_cast<int>(BPCondition_types::Code));
    }
    ar.Serialize("code", code);
}

void BPAction_ExecCode::execute(Debugger* dbg, BreakPoint* bp, const DebuggerInstructionInfo& info) {
    info.context->callStack.back()->EvaluateExpression((code + " " + std::to_string(bp->hitcount) + ";" +
        info.instruction->GetDebugName().data() +
        "\"").c_str(), 10);
}

void BPAction_ExecCode::Serialize(JsonArchive& ar) {
    if (!ar.reading()) {
        ar.Serialize("type", static_cast<int>(BPAction_types::ExecCode));
    }
    ar.Serialize("code", code);
}

extern EngineAlive* EngineAliveFnc;
extern EngineEnableMouse* EngineEnableMouseFnc;

void BPAction_Halt::execute(Debugger* dbg, BreakPoint* bp, const DebuggerInstructionInfo& info) {
    auto waitEvent = std::make_shared<std::condition_variable>();
    std::mutex waitMutex;

    //#TODO catch crashes in these engine funcs by using https://msdn.microsoft.com/en-us/library/1deeycx5(v=vs.80).aspx http://stackoverflow.com/questions/457577/catching-access-violation-exceptions

#ifndef X64 //#TODO crashy bashy
    if (EngineEnableMouseFnc) EngineEnableMouseFnc(false); //Free mouse from Arma
#endif
    dbg->onHalt(waitEvent, bp, info, type);
    bool halting = true;
    while (halting) {
        std::unique_lock<std::mutex> lk(waitMutex);
        auto result = waitEvent->wait_for(lk, 3s);

#ifndef X64 //#TODO crashy bashy
        if (EngineAliveFnc) EngineAliveFnc();
#endif
        if (result != std::cv_status::timeout) {
            halting = false;
            //EngineEnableMouseFnc(true); Causes mouse to jump to bottom right screen corner
            dbg->onContinue();
        }
    }

}

void BPAction_Halt::Serialize(JsonArchive& ar) {
    if (!ar.reading()) {
        ar.Serialize("type", static_cast<int>(BPAction_types::Halt));
    }
}

void BPAction_LogCallstack::execute(Debugger*, BreakPoint* bp, const DebuggerInstructionInfo& instructionInfo) {
    JsonArchive ar;
    instructionInfo.context->Serialize(ar);
    auto text = ar.to_string();
    std::ofstream f(basePath + bp->label + std::to_string(bp->hitcount) + ".json", std::ios::out | std::ios::binary);
    f.write(text.c_str(), text.length());
    f.close();
}

void BPAction_LogCallstack::Serialize(JsonArchive& ar) {
    if (!ar.reading()) {
        ar.Serialize("type", static_cast<int>(BPAction_types::LogCallstack));
    }
    ar.Serialize("basePath", basePath);
}
