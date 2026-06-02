// *************************************************************************
// File       [ atpg.h ]
// Author     [ littleshamoo ]
// Synopsis   [ ]
// Date       [ 2011/11/01 created ]
// **************************************************************************

#ifndef _CORE_ATPG_H_
#define _CORE_ATPG_H_

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector> // added by wang
#include <stack>	// added by wang
#include <algorithm>
#include <unordered_map>
#include "decision_tree.h"
#include "simulator.h"

namespace CoreNs
{
	constexpr int BACKTRACK_LIMIT = 500;
	constexpr int INFINITE = 0x7fffffff;
	constexpr int MAX_LIST_SIZE = 1000;
	constexpr int NO_UNIQUE_PATH = -1;
	constexpr int UNIQUE_PATH_SENSITIZE_FAIL = -2;
	constexpr double STC_PEAK_POWER_LIMIT_RATIO = 1.0;
	constexpr size_t STC_MAX_MERGE_PARTNER_CHECKS = 512;
	constexpr size_t STC_RECOVERY_PATTERN_RETRY_LIMIT = 5;
	constexpr double CAPTURE_PEAK_BUDGET_RATIO = 0.8;
	constexpr double CAPTURE_PEAK_PENALTY_WEIGHT = 4.0;
	constexpr size_t TDF_DTC_MAX_SELECTED_FAULTS = 8;
	constexpr size_t TDF_DTC_MAX_FAILED_ATTEMPTS = 8;
	constexpr double TDF_DTC_MIN_X_DENSITY = 0.20;
	constexpr double TDF_DTC_CANDIDATE_SET_RATIO = 0.5;
	constexpr size_t TDF_DTC_MIN_CANDIDATE_SET_SIZE = 64;
	constexpr size_t TDF_DTC_DIRTY_GATE_RESERVE = 64;
	constexpr size_t TDF_DTC_MAX_CANDIDATE_WINDOW = 512;
	constexpr size_t TDF_DTC_PRIMARY_COMPATIBILITY_WINDOW = 128;

	class Atpg
	{
	public:
		inline Atpg(Circuit *pCircuit, Simulator *pSimulator);

		enum SINGLE_PATTERN_GENERATION_STATUS
		{
			PATTERN_FOUND = 0,
			FAULT_UNTESTABLE,
			ABORT
		};
		enum GATE_LINE_TYPE
		{
			FREE_LINE = 0,
			HEAD_LINE,
			BOUND_LINE
		};
		enum XPATH_STATE
		{
			NO_XPATH_EXIST = 0,
			XPATH_EXIST,
			UNKNOWN
		};
		enum IMPLICATION_STATUS
		{
			FORWARD = 0,
			BACKWARD,
			CONFLICT
		};
		enum BACKTRACE_STATUS
		{
			INITIAL = 0,
			CHECK_AND_SELECT,
			CURRENT_OBJ_DETERMINE,
			FAN_OBJ_DETERMINE
		};
		enum BACKTRACE_RESULT
		{
			NO_CONTRADICTORY = 0,
			CONTRADICTORY
		};

		// class Atpg main method
		void generatePatternSet(PatternProcessor *pPatternProcessor, FaultListExtract *pFaultListExtractor, bool isMFO, bool powerAware = false);

	private:
		Circuit *pCircuit_;																				// the circuit built on read verilog
		Simulator *pSimulator_;																		// the simulator based on the built circuit
		Fault currentTargetFault_;																// current target fault for generateSinglePatternOnTargetFault
		Fault currentTargetHeadLineFault_;												// current equivalent headline fault of target currentTargetFault_
		int numOfheadLines_;																			// number of headlines
		std::vector<int> headLineGateIDs_;												// all the head line gateID in the circuit
		std::vector<int> gateID_to_n0_;														// gateID's n0_ value for multiple backtracing
		std::vector<int> gateID_to_n1_;														// gateID's n1_ value for multiple backtracing
		std::vector<int> gateID_to_valModified_;									// indicate whether the gate has been backtraced or implied, true means the gate has been modified
		std::vector<int> gateID_to_reachableByTargetFault_;				// 1 means this fanout is in fanout cone of target fault, 0 otherwise
		std::vector<GATE_LINE_TYPE> gateID_to_lineType_;					// array of line types for all gates, i.e. FREE, HEAD, BOUND
		std::vector<XPATH_STATE> gateID_to_xPathStatus_;					// gateID to its xPathStatus, i.e. XPATH_EXIST, NO_XPATH_EXIST, UNKNOWN
		std::vector<int> xPathTouchedGateIDs_;									// gates whose xPathStatus_ was changed from UNKNOWN
		std::vector<std::vector<int>> gateID_to_uniquePath_;			// list of gates on the unique path associated with a D-frontier, when there is only one gate in D-frontier, xPathTracing will update this information.
		std::vector<std::vector<int>> circuitLevel_to_EventStack_; // every circuit level has its own corresponding event stack
		DecisionTree backtrackDecisionTree_;											// the whole tree store the order for later possible backtracking, DecisionTreeNode store the starting point in backtrackImplicatedGateIDs_
		std::vector<int> backtrackImplicatedGateIDs_;							// backtrackImplicatedGateIDs_[backTrackPoint] = start point of associated gateID
		std::vector<int> gateIDsToResetAfterBackTrace_;						// the gate that need to have gate.n0_ and gate.n1_ reset after backtrace
		std::vector<int> initialObjectives_;											// initial fault activation objectives or D-frontier propagation objectives.
		std::vector<int> currentObjectives_;											// the objectives that are selected from final objectives.
		std::vector<int> fanoutObjectives_;												// the fanout objectives recorded in atpg
		std::vector<int> headLineObjectives_;											// objectives at the headlines
		std::vector<int> finalObjectives_;												// final objectives include fanout objectives and headline objectives.
		std::vector<int> unjustifiedGateIDs_;											// J-frontier list
		std::vector<int> dFrontiers_;															// D-frontier list
		std::vector<int> isInEventStack_;													// 1 if a gate is in an event stack, 0 otherwise
		struct InfluenceLiteral
		{
			int inputIndex_;
			unsigned char valueMask_;
		};
		struct InfluenceSet
		{
			std::vector<InfluenceLiteral> literals_;
			bool impossible_;
			InfluenceSet() : impossible_(false) {}
		};
		bool tdfInfluenceMeasuresValid_;
		bool useInfluenceInputBacktrace_;
		bool influencePrimaryOrderValid_;
		size_t influencePrimaryCursor_;
		std::vector<int> influenceInputIndexByGate_;
		std::vector<int> influenceInputGateIDs_;
		std::vector<unsigned char> influenceSpecifiedInputValues_;
		std::vector<int> influenceSpecifiedInputIndices_;
		std::vector<InfluenceSet> influenceRc0_;
		std::vector<InfluenceSet> influenceRc1_;
		std::vector<InfluenceSet> influenceRo_;
		std::unordered_map<const Fault *, InfluenceSet> influenceDetCache_;
		std::unordered_map<const Fault *, InfluenceSet> influenceStructuralCache_;
		std::vector<Fault *> influencePrimaryOrder_;
		std::vector<int> influenceStructuralVisitedStamp_;
		std::vector<int> influenceStructuralForwardStamp_;
		std::vector<int> influenceStructuralStack_;
		std::vector<int> influenceStructuralInputIndices_;
		int influenceStructuralStamp_;
		size_t tdfDtcPatternCount_;
		size_t tdfDtcSelectedFaults_;
		size_t tdfDtcAttemptedCandidates_;
		size_t tdfDtcRejectedCandidates_;
		size_t tdfDtcDetectedDrops_;
		size_t tdfDtcNewSpecifiedBits_;
		std::vector<size_t> powerFillPiImpact_;									// cached response impact for each PI
		std::vector<int> powerFillPiOrder_;											// PIs sorted by descending response impact
		std::vector<std::vector<int>> powerFillPi1FanoutCones_;			// affected gates for incremental PI1 power fill simulation
		std::vector<std::vector<int>> powerFillPi2FanoutCones_;			// affected gates for incremental PI2 power fill simulation
		Gate *firstTimeFrameHeadLine_;														// this parameter is for multiple time frame

		// ---------------private methods----------------- //

		void setupCircuitParameter();
		void calculateGateDepthFromPO();
		void identifyGateLineType();
		void identifyGateDominator();
		void identifyGateUniquePath();

		void TransitionDelayFaultATPG(FaultPtrList &faultPtrListForGen, PatternProcessor *pPatternProcessor, int &numOfAtpgUntestableFaults, bool powerAwareSTC = false);
		void StuckAtFaultATPG(FaultPtrList &faultListToGen, PatternProcessor *pPatternProcessor, int &numOfAtpgUntestableFaults);

		Gate *getGateForFaultActivation(const Fault &fault);
		void setGateAtpgValAndRunImplication(Gate &gate, const Value &val, std::vector<int> *modifiedGateIDs = NULL, std::vector<char> *modifiedGateMarks = NULL);

		void resetPrevAtpgValStored();
		int storeCurrentAtpgVal();
		void clearAllFaultEffectByEvaluation();
		void clearFaultEffectOnGateAtpgVal(Gate &gate);

		SINGLE_PATTERN_GENERATION_STATUS generateSinglePatternOnTargetFault(Fault targetFault, bool isAtStageDTC);

		// initialization at the start of single pattern generation
		Gate *initializeForSinglePatternGeneration(Fault &targetFault, int &BackImpLevel, IMPLICATION_STATUS &implicationStatus, const bool &isAtStageDTC);
		void initializeObjectivesAndFrontiers();
		void initializeCircuitWithFaultyGate(Gate &gFaultyLine, bool isAtStageDTC);

		bool applyTransitionDelayDynamicCompression(FaultPtrList &faultPtrListForGen, Pattern &pattern);
		Fault makeTransitionDelayTargetFault(const Fault &originalFault) const;

		void clearEventStack(bool isDebug);
		bool doImplication(IMPLICATION_STATUS atpgStatus, int startLevel);
		IMPLICATION_STATUS doOneGateBackwardImplication(Gate *pGate);

		bool backtrack(int &BackImpLevel);
		bool continuationMeaningful(Gate *pLastDFrontier);
		void updateUnjustifiedGateIDs();
		void updateDFrontiers();
		bool checkIfFaultHasPropagatedToPO(bool &faultHasPropagatedToPO);
		bool checkForUnjustifiedBoundLines();
		void findFinalObjective(BACKTRACE_STATUS &backtraceFlag, const bool &faultCanPropToPO, Gate *&pLastDFrontier);
		void clearAllObjectives();
		void assignAtpgValToFinalObjectiveGates();
		void justifyFreeLines(Fault &originalFault);
		void restoreFault(Fault &originalFault);
		int countEffectiveDFrontiers(Gate *pFaultyLineGate);
		int doUniquePathSensitization(Gate &gate);

		bool xPathExists(Gate *pGate);
		bool xPathTracing(Gate *pGate);

		int setFaultyGate(Fault &fault);
		Fault setFreeLineFaultyGate(Gate &gate);

		void fanoutFreeBacktrace(Gate *pGate);
		BACKTRACE_RESULT multipleBacktrace(BACKTRACE_STATUS atpgStatus, int &possibleFinalObjectiveID);
		Value assignBacktraceValue(int &n0, int &n1, const Gate &gate);
		void initializeForMultipleBacktrace();
		Gate *findEasiestInput(Gate *pGate, Value atpgValOfpGate);
		Gate *findClosestToPO(std::vector<int> &gateVec, int &index);

		IMPLICATION_STATUS evaluateAndSetGateAtpgVal(Gate *pGate);
		IMPLICATION_STATUS evaluateAndSetFaultyGateAtpgVal(Gate *pGate);

		// static test compression
		void faultSimUndetectedFaultsWithPatternSet(PatternProcessor *pPatternProcessor, FaultPtrList &faults);
		void staticTestCompressionByReverseFaultSimulation(PatternProcessor *pPatternProcessor, FaultPtrList &originalFaultList);
		void staticTestCompression(PatternProcessor *pPatternProcessor, FaultPtrList &originalFaultList, int &numOfAtpgUntestableFaults);
		size_t compactCompatibleTestCubes(PatternProcessor *pPatternProcessor, bool preservePowerLimit);
		void refreshXidDontCareMasks(PatternProcessor *pPatternProcessor);
		void sortPatternsForXID(PatternProcessor *pPatternProcessor);
		void XIDRelaxPatternSet(PatternProcessor *pPatternProcessor, const std::vector<Fault *> &faults);

		int setUpFirstTimeFrame(Fault &fault); // this function is for multiple time frame

		void initializeTdfInfluenceMeasures();
		const InfluenceSet &getInfluenceControlSet(const int &gateID, const Value &value) const;
		InfluenceSet makeInfluenceLiteralSet(const int &gateID, const Value &value) const;
		InfluenceSet unionInfluenceSets(const InfluenceSet &lhs, const InfluenceSet &rhs) const;
		InfluenceSet chooseSmallerInfluenceSet(const InfluenceSet &lhs, const InfluenceSet &rhs) const;
		const InfluenceSet &getTransitionFaultDetectability(const Fault &fault);
		const InfluenceSet &getTransitionFaultStructuralInfluence(const Fault &fault);
		InfluenceSet computeTransitionFaultDetectability(const Fault &fault);
		InfluenceSet computeTransitionFaultStructuralInfluence(const Fault &fault);
		bool influenceSetHasBothValueRequirement(const InfluenceSet &set) const;
		void buildInfluencePrimaryOrder(const FaultPtrList &faultPtrList);
		FaultPtrListIter selectInfluencePrimaryFault(FaultPtrList &faultPtrList);
		void updateInfluenceSpecifiedInputsFromCircuit();
		int countInfluenceOverlap(const InfluenceSet &set) const;
		int countInfluenceConflicts(const InfluenceSet &set) const;
		Gate *findInfluenceInput(Gate *pGate, Value atpgValOfpGate);

		inline Value evaluateGoodVal(Gate &gate);
		inline Value evaluateFaultyVal(Gate &gate);

		inline void setGaten0n1(const int &gateID, const int &n0, const int &n1);

		inline void writeAtpgValToPatternPI(Pattern &pattern);		// write PI values to pattern
		inline void writeGoodSimValToPatternPO(Pattern &pattern); // write PO values to pattern

		inline void pushGateToEventStack(const int &gateID);			 // push events to the event stack of corresponding level
		inline int popEventStack(const int &level);								 // pop and return from eventStack[level]
		inline int pushGateFanoutsToEventStack(const int &gateID); // push all the gate's output to event stack and return pushed gate count

		inline void clearAllEvents();

		inline int vecPop(std::vector<int> &vec);
		inline void vecDelete(std::vector<int> &list, const size_t &index);

		// 5-Value logic evaluation functions
		inline Value cINV(const Value &i1);
		inline Value cAND2(const Value &i1, const Value &i2);
		inline Value cAND3(const Value &i1, const Value &i2, const Value &i3);
		inline Value cAND4(const Value &i1, const Value &i2, const Value &i3, const Value &i4);
		inline Value cNAND2(const Value &i1, const Value &i2);
		inline Value cNAND3(const Value &i1, const Value &i2, const Value &i3);
		inline Value cNAND4(const Value &i1, const Value &i2, const Value &i3, const Value &i4);
		inline Value cOR2(const Value &i1, const Value &i2);
		inline Value cOR3(const Value &i1, const Value &i2, const Value &i3);
		inline Value cOR4(const Value &i1, const Value &i2, const Value &i3, const Value &i4);
		inline Value cNOR2(const Value &i1, const Value &i2);
		inline Value cNOR3(const Value &i1, const Value &i2, const Value &i3);
		inline Value cNOR4(const Value &i1, const Value &i2, const Value &i3, const Value &i4);
		inline Value cXOR2(const Value &i1, const Value &i2);
		inline Value cXOR3(const Value &i1, const Value &i2, const Value &i3);
		inline Value cXNOR2(const Value &i1, const Value &i2);
		inline Value cXNOR3(const Value &i1, const Value &i2, const Value &i3);

		// X-Fill helper methods
		void randomFill(Pattern &pattern);
		void adjacentFill(Pattern &pattern);
		void initializePowerFillOrder();
		void postProcessPowerFill(PatternProcessor *pPatternProcessor);
		void reorderPatternsForShiftPower(PatternProcessor *pPatternProcessor);
		void xFillPattern(PatternProcessor *pPatternProcessor, Pattern &pattern);
		void XFill(PatternProcessor *pPatternProcessor); // final X-fill pass for compressed pattern sets

		// function not used or removed
		void checkLevelInfo();																		// for debug use
		std::string getValStr(Value val);													// for debug use
		void calSCOAP();																					// heuristic not effective, currently not used, added by Wang Wei-Shen
		void testClearFaultEffect(FaultPtrList &faultListToTest); // removed from generatePatternSet() for now seems like debug usage
		void resetIsInEventStack();																// not used
	};

	// --------------inline methods----------------- //
	inline Atpg::Atpg(Circuit *pCircuit, Simulator *pSimulator)
			: pCircuit_(pCircuit),
				pSimulator_(pSimulator),
				gateID_to_n0_(pCircuit->totalGate_, 0),
				gateID_to_n1_(pCircuit->totalGate_, 0),
				gateID_to_valModified_(pCircuit->totalGate_, 0),
				gateID_to_reachableByTargetFault_(pCircuit->totalGate_),
				gateID_to_lineType_(pCircuit->totalGate_, FREE_LINE),
				gateID_to_xPathStatus_(pCircuit->totalGate_, UNKNOWN),
				gateID_to_uniquePath_(pCircuit->totalGate_, std::vector<int>()),
				circuitLevel_to_EventStack_(pCircuit->totalLvl_)
	{
		initialObjectives_.reserve(MAX_LIST_SIZE);
		currentObjectives_.reserve(MAX_LIST_SIZE);
		fanoutObjectives_.reserve(MAX_LIST_SIZE);
		headLineObjectives_.reserve(MAX_LIST_SIZE);
		finalObjectives_.reserve(MAX_LIST_SIZE);

		unjustifiedGateIDs_.reserve(MAX_LIST_SIZE);
		dFrontiers_.reserve(MAX_LIST_SIZE);
		xPathTouchedGateIDs_.reserve(MAX_LIST_SIZE);
		backtrackImplicatedGateIDs_.reserve(pCircuit->totalGate_);
		firstTimeFrameHeadLine_ = NULL;
		isInEventStack_.resize(pCircuit->totalGate_);
		tdfInfluenceMeasuresValid_ = false;
		useInfluenceInputBacktrace_ = false;
		influencePrimaryOrderValid_ = false;
		influencePrimaryCursor_ = 0;
		influenceStructuralStamp_ = 0;
		tdfDtcPatternCount_ = 0;
		tdfDtcSelectedFaults_ = 0;
		tdfDtcAttemptedCandidates_ = 0;
		tdfDtcRejectedCandidates_ = 0;
		tdfDtcDetectedDrops_ = 0;
		tdfDtcNewSpecifiedBits_ = 0;
	}

	// **************************************************************************
	// Function   [ Atpg::evaluateGoodVal ]
	// Commenter  [ WYH WWS ]
	// Synopsis   [ usage: Given the gate without fault, and generate the output,
	//                     and return.
	//
	//              arguments:
	// 								[in] gate: The gate to evaluate.
	//
	//              output: Evaluated value.
	//            ]
	// Date       [ WYH Ver. 1.0 started 2013/08/15 last modified 2023/01/06]
	// **************************************************************************
	inline Value Atpg::evaluateGoodVal(Gate &gate)
	{
		if (gate.gateType_ == Gate::PI || gate.gateType_ == Gate::PPI)
		{
			return gate.atpgVal_;
		}

		static Value v[4];
		int index = 0;
		for (const int &faninID : gate.faninVector_)
		{
			v[index++] = pCircuit_->circuitGates_[faninID].atpgVal_;
		}

		switch (gate.gateType_)
		{
			case Gate::PO:
			case Gate::PPO:
			case Gate::BUF:
				return v[0];
			case Gate::INV:
				return cINV(v[0]);
			case Gate::AND2:
				return cAND2(v[0], v[1]);
			case Gate::AND3:
				return cAND3(v[0], v[1], v[2]);
			case Gate::AND4:
				return cAND4(v[0], v[1], v[2], v[3]);
			case Gate::NAND2:
				return cNAND2(v[0], v[1]);
			case Gate::NAND3:
				return cNAND3(v[0], v[1], v[2]);
			case Gate::NAND4:
				return cNAND4(v[0], v[1], v[2], v[3]);
			case Gate::OR2:
				return cOR2(v[0], v[1]);
			case Gate::OR3:
				return cOR3(v[0], v[1], v[2]);
			case Gate::OR4:
				return cOR4(v[0], v[1], v[2], v[3]);
			case Gate::NOR2:
				return cNOR2(v[0], v[1]);
			case Gate::NOR3:
				return cNOR3(v[0], v[1], v[2]);
			case Gate::NOR4:
				return cNOR4(v[0], v[1], v[2], v[3]);
			case Gate::XOR2:
				return cXOR2(v[0], v[1]);
			case Gate::XOR3:
				return cXOR3(v[0], v[1], v[2]);
			case Gate::XNOR2:
				return cXNOR2(v[0], v[1]);
			case Gate::XNOR3:
				return cXNOR3(v[0], v[1], v[2]);
			default:
				return v[0];
		}
	}

	// **************************************************************************
	// Function   [ Atpg::evaluateFaultyVal ]
	// Commenter  [ CAL WWS ]
	// Synopsis   [ usage: Given the gate with fault, and generate the output,
	//                     and return.
	//
	//              arguments:
	// 								[in] gate: The gate to evaluate.
	//
	//              output: Evaluated value.
	//            ]
	// Date       [ Ver. 1.0 started 2013/08/13 last modified 2023/01/06 ]
	// **************************************************************************
	inline Value Atpg::evaluateFaultyVal(Gate &gate)
	{
		Value val;
		const int &faultyLine = currentTargetFault_.faultyLine_;
		switch (gate.gateType_)
		{
			case Gate::PI:
			case Gate::PPI:
				return gate.atpgVal_;
			case Gate::PO:
			case Gate::PPO:
			case Gate::BUF:
				val = pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_;
				if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
				{
					val = B; // logic D' (0/1)
				}
				if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
				{
					val = D; // logic D  (1/0)
				}
				return val;
			case Gate::INV:
				val = pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_;
				if (faultyLine == 0)
				{
					val = cINV(val);
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
					return val;
				}
				else
				{
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
					return cINV(val);
				}
			case Gate::AND2:
			case Gate::AND3:
			case Gate::AND4:
				if (faultyLine == 0)
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_;
					for (int i = 1; i < gate.numFI_; ++i)
					{
						val = cAND2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
					}

					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}

					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
				}
				else
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[faultyLine - 1]].atpgVal_;
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}

					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}

					for (int i = 0; i < gate.numFI_; ++i)
					{
						if (i != faultyLine - 1)
						{
							val = cAND2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
						}
					}
				}
				return val;
			case Gate::NAND2:
			case Gate::NAND3:
			case Gate::NAND4:
				if (faultyLine == 0)
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_;
					for (int i = 1; i < gate.numFI_; ++i)
					{
						val = cAND2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
					}

					val = cINV(val);
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}

					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
				}
				else
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[faultyLine - 1]].atpgVal_;
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}

					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}

					for (int i = 0; i < gate.numFI_; ++i)
					{
						if (i != faultyLine - 1)
						{
							val = cAND2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
						}
					}
					val = cINV(val);
				}
				return val;
			case Gate::OR2:
			case Gate::OR3:
			case Gate::OR4:
				if (faultyLine == 0)
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_;
					for (int i = 1; i < gate.numFI_; ++i)
					{
						val = cOR2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
					}

					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}

					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
				}
				else
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[faultyLine - 1]].atpgVal_;
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}

					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}

					for (int i = 0; i < gate.numFI_; ++i)
					{
						if (i != faultyLine - 1)
						{
							val = cOR2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
						}
					}
				}
				return val;
			case Gate::NOR2:
			case Gate::NOR3:
			case Gate::NOR4:
				if (faultyLine == 0)
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_;
					for (int i = 1; i < gate.numFI_; ++i)
					{
						val = cOR2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
					}

					val = cINV(val);

					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
				}
				else
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[faultyLine - 1]].atpgVal_;
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
					for (int i = 0; i < gate.numFI_; ++i)
					{
						if (i != faultyLine - 1)
						{
							val = cOR2(val, pCircuit_->circuitGates_[gate.faninVector_[i]].atpgVal_);
						}
					}
					val = cINV(val);
				}
				return val;
			case Gate::XOR2:
			case Gate::XOR3:
				if (faultyLine == 0)
				{
					if (gate.gateType_ == Gate::XOR2)
					{
						val = cXOR2(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_);
					}
					else
					{
						val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_);
					}

					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
				}
				else
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[faultyLine - 1]].atpgVal_;
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}

					if (gate.gateType_ == Gate::XOR2)
					{
						// if (faultyLine - 1 == 0)
						// 	val = cXOR2(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, val);
						// else
						// 	val = cXOR2(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
						switch (faultyLine - 1)
						{
							case 0:
								val = cXOR2(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, val);
								break;
							case 1:
								val = cXOR2(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
								break;
							default:
								std::cerr << "switch case default, should not happen\n";
								break;
						}
					}
					else
					{ // XOR3
						// if (faultyLine - 1 == 0)
						// 	val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
						// else if (faultyLine - 1 == 1)
						// 	val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
						// else
						// 	val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
						switch (faultyLine - 1)
						{
							case 0:
								val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
								break;
							case 1:
								val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
								break;
							case 2:
								val = cXOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
								break;
							default:
								std::cerr << "switch case default, should not happen\n";
								break;
						}
					}
				}
				return val;
			case Gate::XNOR2:
			case Gate::XNOR3:
				if (faultyLine == 0)
				{
					if (gate.gateType_ == Gate::XNOR2)
					{
						val = cXNOR2(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_);
					}
					else
					{
						val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_);
					}

					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}
				}
				else
				{
					val = pCircuit_->circuitGates_[gate.faninVector_[faultyLine - 1]].atpgVal_;
					if (val == L && (currentTargetFault_.faultType_ == Fault::SA1 || currentTargetFault_.faultType_ == Fault::STF))
					{
						val = B;
					}
					if (val == H && (currentTargetFault_.faultType_ == Fault::SA0 || currentTargetFault_.faultType_ == Fault::STR))
					{
						val = D;
					}

					if (gate.gateType_ == Gate::XNOR2)
					{
						// if (faultyLine - 1 == 0)
						// 	val = cXNOR2(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, val);
						// else
						// 	val = cXNOR2(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
						switch (faultyLine - 1)
						{
							case 0:
								val = cXNOR2(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, val);
								break;
							case 1:
								val = cXNOR2(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
								break;
							default:
								std::cerr << "switch case default, should not happen\n";
								break;
						}
					}
					else
					{ // XOR3
						// change from if else to switch by wang
						// if (faultyLine - 1 == 0)
						// 	val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
						// else if (faultyLine - 1 == 1)
						// 	val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
						// else
						// 	val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
						switch (faultyLine - 1)
						{
							case 0:
								val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
								break;
							case 1:
								val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[2]].atpgVal_, val);
								break;
							case 2:
								val = cXNOR3(pCircuit_->circuitGates_[gate.faninVector_[1]].atpgVal_, pCircuit_->circuitGates_[gate.faninVector_[0]].atpgVal_, val);
								break;
							default:
								std::cerr << "switch case default, should not happen\n";
								break;
						}
					}
				}
				return val;
			default:
				return gate.atpgVal_;
		}
	}

	inline void Atpg::setGaten0n1(const int &gateID, const int &n0, const int &n1)
	{
		gateID_to_n0_[gateID] = n0;
		gateID_to_n1_[gateID] = n1;
	}

	// **************************************************************************
	// Function   [ Atpg::writeAtpgValToPatternPI ]
	// Commenter  [ CAL WWS ]
	// Synopsis   [ usage: Assign atpgVal_ of PI/PPI to PI/PPI in pattern.
	//
	// 							arguments:
	// 								[in, out] pattern: An empty pattern to be set.
	//            ]
	// Date       [ Ver. 1.0 started 2013/08/13  last modified 2023/01/06 ]
	// **************************************************************************
	inline void Atpg::writeAtpgValToPatternPI(Pattern &pattern)
	{
		for (int i = 0; i < pCircuit_->numPI_; ++i)
		{
			pattern.PI1_[i] = pCircuit_->circuitGates_[i].atpgVal_;
		}
		// if (pattern.PI2_ != NULL && pCircuit_->numFrame_ > 1)
		if (!(pattern.PI2_.empty()) && pCircuit_->numFrame_ > 1)
		{
			for (int i = 0; i < pCircuit_->numPI_; ++i)
			{
				pattern.PI2_[i] = pCircuit_->circuitGates_[i + pCircuit_->numGate_].atpgVal_;
			}
		}
		for (int i = 0; i < pCircuit_->numPPI_; ++i)
		{
			pattern.PPI_[i] = pCircuit_->circuitGates_[pCircuit_->numPI_ + i].atpgVal_;
		}
		// if (pattern.SI_ != NULL && pCircuit_->numFrame_ > 1)
		if (!(pattern.SI_.empty()) && pCircuit_->numFrame_ > 1)
		{
			pattern.SI_[0] = (pCircuit_->timeFrameConnectType_ == Circuit::SHIFT) ? pCircuit_->circuitGates_[pCircuit_->numGate_ + pCircuit_->numPI_].atpgVal_ : X;
		}
	}

	// **************************************************************************
	// Function   [ Atpg::writeGoodSimValToPatternPO ]
	// Commenter  [ CAL WWS ]
	// Synopsis   [ usage: assign atpgVal_ of PO/PPO to PP/PPO in pattern
	//
	// 							arguments:
	// 								[in, out] pattern:
	// 									An empty pattern to be set.
	//            ]
	// Date       [ Ver. 1.0 started 2013/08/13 last modified 2023/01/06 ]
	// **************************************************************************
	inline void Atpg::writeGoodSimValToPatternPO(Pattern &pattern)
	{
		int offset = pCircuit_->numGate_ - pCircuit_->numPO_ - pCircuit_->numPPI_;
		for (int i = 0; i < pCircuit_->numPO_; ++i)
		{
			if (pCircuit_->circuitGates_[offset + i].goodSimLow_ == PARA_H)
			{
				pattern.PO1_[i] = L;
			}
			else if (pCircuit_->circuitGates_[offset + i].goodSimHigh_ == PARA_H)
			{
				pattern.PO1_[i] = H;
			}
			else
			{
				pattern.PO1_[i] = X;
			}
		}

		if (!(pattern.PO2_.empty()) && pCircuit_->numFrame_ > 1)
		{
			for (int i = 0; i < pCircuit_->numPO_; ++i)
			{
				if (pCircuit_->circuitGates_[offset + i + pCircuit_->numGate_].goodSimLow_ == PARA_H)
				{
					pattern.PO2_[i] = L;
				}
				else if (pCircuit_->circuitGates_[offset + i + pCircuit_->numGate_].goodSimHigh_ == PARA_H)
				{
					pattern.PO2_[i] = H;
				}
				else
				{
					pattern.PO2_[i] = X;
				}
			}
		}

			offset = pCircuit_->numGate_ - pCircuit_->numPPI_;
			if (!(pattern.PPO1_.empty()) && pCircuit_->numFrame_ > 1)
			{
				for (int i = 0; i < pCircuit_->numPPI_; ++i)
				{
					if (pCircuit_->circuitGates_[offset + i].goodSimLow_ == PARA_H)
					{
						pattern.PPO1_[i] = L;
					}
					else if (pCircuit_->circuitGates_[offset + i].goodSimHigh_ == PARA_H)
					{
						pattern.PPO1_[i] = H;
					}
					else
					{
						pattern.PPO1_[i] = X;
					}
				}
			}
			if (pCircuit_->numFrame_ > 1)
			{
				offset += pCircuit_->numGate_ * (pCircuit_->numFrame_ - 1);
			}

			for (int i = 0; i < pCircuit_->numPPI_; ++i)
		{
			if (pCircuit_->circuitGates_[offset + i].goodSimLow_ == PARA_H)
			{
				pattern.PPO_[i] = L;
			}
			else if (pCircuit_->circuitGates_[offset + i].goodSimHigh_ == PARA_H)
			{
				pattern.PPO_[i] = H;
			}
			else
			{
				pattern.PPO_[i] = X;
			}
		}
	}

	inline void Atpg::pushGateToEventStack(const int &gateID)
	{
		const Gate &gate = pCircuit_->circuitGates_[gateID];
		if (!gateID_to_valModified_[gateID])
		{
			circuitLevel_to_EventStack_[gate.numLevel_].push_back(gateID);
			gateID_to_valModified_[gateID] = 1;
		}
	}

	inline int Atpg::popEventStack(const int &level)
	{
		const int gateID = circuitLevel_to_EventStack_[level].back();
		circuitLevel_to_EventStack_[level].pop_back();
		return gateID;
	}

	inline int Atpg::pushGateFanoutsToEventStack(const int &gateID)
	{
		int pushedGateCount = 0;
		const Gate &gate = pCircuit_->circuitGates_[gateID];
		for (const int &fanoutID : gate.fanoutVector_)
		{
			const int &outputGateLevel = pCircuit_->circuitGates_[fanoutID].numLevel_;
			if (!gateID_to_valModified_[fanoutID])
			{
				circuitLevel_to_EventStack_[outputGateLevel].push_back(fanoutID);
				gateID_to_valModified_[fanoutID] = 1;
				++pushedGateCount;
			}
		}
		return pushedGateCount;
	}

	inline void Atpg::clearAllEvents()
	{
		for (std::vector<int> &eventStack : circuitLevel_to_EventStack_)
		{
			for (const int &gateID : eventStack)
			{
				gateID_to_valModified_[gateID] = 0;
			}
			eventStack.clear();
		}
	}

	inline int Atpg::vecPop(std::vector<int> &vec) // listPop => vecPop by wang
	{
		int lastElement = vec.back();
		vec.pop_back();
		return lastElement;
	}

	inline void Atpg::vecDelete(std::vector<int> &list, const size_t &index)
	{
		list[index] = list.back();
		list.pop_back();
	}

	// 5-value logic evaluation functions
	inline Value Atpg::cINV(const Value &i1)
	{
		constexpr Value map[5] = {H, L, X, B, D};
		if (i1 >= Z)
		{
			return Z;
		}
		return map[i1];
	}
	inline Value Atpg::cAND2(const Value &i1, const Value &i2)
	{
		constexpr Value map[5][5] = {
				{L, L, L, L, L},
				{L, H, X, D, B},
				{L, X, X, X, X},
				{L, D, X, D, L},
				{L, B, X, L, B}};
		if (i1 >= Z || i2 >= Z)
		{
			return Z;
		}
		return map[i1][i2];
	}
	inline Value Atpg::cAND3(const Value &i1, const Value &i2, const Value &i3)
	{
		return cAND2(i1, cAND2(i2, i3));
	}
	inline Value Atpg::cAND4(const Value &i1, const Value &i2, const Value &i3, const Value &i4)
	{
		return cAND2(cAND2(i1, i2), cAND2(i3, i4));
	}
	inline Value Atpg::cNAND2(const Value &i1, const Value &i2)
	{
		return cINV(cAND2(i1, i2));
	}
	inline Value Atpg::cNAND3(const Value &i1, const Value &i2, const Value &i3)
	{
		return cINV(cAND3(i1, i2, i3));
	}
	inline Value Atpg::cNAND4(const Value &i1, const Value &i2, const Value &i3, const Value &i4)
	{
		return cINV(cAND4(i1, i2, i3, i4));
	}
	inline Value Atpg::cOR2(const Value &i1, const Value &i2)
	{
		constexpr Value map[5][5] = {
				{L, H, X, D, B},
				{H, H, H, H, H},
				{X, H, X, X, X},
				{D, H, X, D, H},
				{B, H, X, H, B}};
		if (i1 >= Z || i2 >= Z)
		{
			return Z;
		}
		return map[i1][i2];
	}
	inline Value Atpg::cOR3(const Value &i1, const Value &i2, const Value &i3)
	{
		return cOR2(i1, cOR2(i2, i3));
	}
	inline Value Atpg::cOR4(const Value &i1, const Value &i2, const Value &i3, const Value &i4)
	{
		return cOR2(cOR2(i1, i2), cOR2(i3, i4));
	}
	inline Value Atpg::cNOR2(const Value &i1, const Value &i2)
	{
		return cINV(cOR2(i1, i2));
	}
	inline Value Atpg::cNOR3(const Value &i1, const Value &i2, const Value &i3)
	{
		return cINV(cOR3(i1, i2, i3));
	}
	inline Value Atpg::cNOR4(const Value &i1, const Value &i2, const Value &i3, const Value &i4)
	{
		return cINV(cOR4(i1, i2, i3, i4));
	}
	inline Value Atpg::cXOR2(const Value &i1, const Value &i2)
	{
		constexpr Value map[5][5] = {
				{L, H, X, D, B},
				{H, L, X, B, D},
				{X, X, X, X, X},
				{D, B, X, L, H},
				{B, D, X, H, L}};
		if (i1 >= Z || i2 >= Z)
		{
			return Z;
		}
		return map[i1][i2];
	}
	inline Value Atpg::cXOR3(const Value &i1, const Value &i2, const Value &i3)
	{
		return cXOR2(i1, cXOR2(i2, i3));
	}
	inline Value Atpg::cXNOR2(const Value &i1, const Value &i2)
	{
		return cINV(cXOR2(i1, i2));
	}
	inline Value Atpg::cXNOR3(const Value &i1, const Value &i2, const Value &i3)
	{
		return cINV(cXOR3(i1, i2, i3));
	}


};

#endif
