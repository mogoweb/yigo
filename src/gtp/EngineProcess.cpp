#include "EngineProcess.h"

#include "EngineProcess.h"
void EngineProcess::onReadyRead() {}
EngineProcess::EngineProcess(QObject* parent) : QObject(parent) {}
EngineProcess::~EngineProcess() = default;
bool EngineProcess::start(const EngineConfig&) { return false; }
void EngineProcess::stop() {}
bool EngineProcess::isRunning() const { return false; }
bool EngineProcess::isAnalyzing() const { return false; }
void EngineProcess::query(quint64, const QString&) {}
void EngineProcess::startAnalysis(const AnalysisQuery&) {}
void EngineProcess::stopAnalysis() {}
void EngineProcess::handleResponse(quint64, bool, const QString&) {}
void EngineProcess::doStartAnalysis() {}
