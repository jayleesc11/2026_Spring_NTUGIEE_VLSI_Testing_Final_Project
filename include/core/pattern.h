// **************************************************************************
// File       [ pattern.h ]
// Author     [ littleshamoo ]
// Synopsis   [ ]
// Date       [ 2011/09/13 created ]
// **************************************************************************

#ifndef _CORE_PATTERN_H_
#define _CORE_PATTERN_H_

#include <vector>
#include <map>
#include <utility>
#include "circuit.h"
#include "fault.h"

namespace CoreNs
{
	class Pattern
	{
	public:
		inline Pattern() {}
		inline Pattern(Circuit *pCircuit);
		std::vector<Value> PI1_;
		std::vector<Value> PI2_;
		std::vector<Value> PPI_;
		std::vector<Value> SI_;
		std::vector<Value> PO1_;
		std::vector<Value> PO2_;
		std::vector<Value> PPO1_;
		std::vector<Value> PPO_;
		std::vector<bool> captureClockEnabled_;
		inline void initForTransitionDelayFault(Circuit *pCircuit);
	};
	inline Pattern::Pattern(Circuit *pCircuit)
			: PI1_(pCircuit->numPI_),
				PPI_(pCircuit->numPPI_),
				PO1_(pCircuit->numPO_),
				PPO_(pCircuit->numPPI_){};

	// **************************************************************************
	// Function   [ Pattern::initForTransitionDelayFault ]
	// Commenter  [ CHT ]
	// Synopsis   [ usage: Basic setup initialization.
	//							description:
	// 								Resize vectors PI2_ and PO2_ to numPI_ and numPO_ of the
	// 								Circuit, and resize SI_ to 1.
	//							argument:
	//								[in] pCircuit : The pointer to the target Circuit.
	//						]
	// Date       [ CHT Ver. 1.0 started  2023/01/05 ]
	// **************************************************************************
	inline void Pattern::initForTransitionDelayFault(Circuit *pCircuit)
		{
			PI2_.resize(pCircuit->numPI_);
			PO2_.resize(pCircuit->numPO_);
			PPO1_.resize(pCircuit->numPPI_);
			SI_.resize(1);
		}

	// This class process the test pattern set
	// it supports :
	//  X-filling
	//  static compression
	class PatternProcessor
	{
	public:
		// BASIC_SCAN is for single time frame, stuck-at fault
		// LAUNCH_CAPTURE is for 2-pattern test , launch-on-capture
		// LAUNCH_SHIFT is for 2-pattern test, launch-on-shift
		enum Type
		{
			BASIC_SCAN = 0,
			LAUNCH_CAPTURE,
			LAUNCH_SHIFT
		};

		inline PatternProcessor();
		enum State
		{
			OFF = 0,
			ON
		};
		enum XFillMode
		{
			XFILL_RANDOM,
			XFILL_ADJACENT,
			XFILL_SFILL,
			XFILL_POWER
		};

		struct CaptureFFTCStats
		{
			inline CaptureFFTCStats();
			size_t patternCount_;
			size_t scanFlipFlopCount_;
			size_t captureCycleCount_;
			size_t totalCaptureFFTC_;
			size_t maxCaptureFFTC_;
			size_t totalUnknownCapturePairs_;
			double averageCaptureFFTC_;
		};

		struct ShiftFFTCStats
		{
			inline ShiftFFTCStats();
			size_t patternCount_;
			size_t scanFlipFlopCount_;
			size_t shiftOperationCount_;
			size_t shiftCycleCount_;
			size_t totalShiftFFTC_;
			size_t maxShiftFFTC_;
			size_t totalUnknownShiftPairs_;
			double averageShiftFFTC_;
		};

		State staticCompression_;	 // Flag; ON = need static compression
		State dynamicCompression_; // Flag; ON = need dynamic compression
		State XFill_;							 // Flag; ON = need X-Filling
		XFillMode XFillMode_;		 // X-Fill strategy when XFill_ is ON.

		Type type_;
		int numPI_;
		int numPPI_;
		int numSI_;
		int numPO_;
		std::vector<Pattern> patternVector_;
		std::vector<int> pPIorder_;
		std::vector<int> pPPIorder_;
		std::vector<int> pPOorder_;

		void init(Circuit *pCircuit);
		CaptureFFTCStats calculateCaptureFFTCStats() const;
		ShiftFFTCStats calculateShiftFFTCStats(const bool includeFinalUnload = false) const;
		// Fixed static compression
		void StaticCompression();
		bool updateTable(std::vector<bool> mergeRecord, std::vector<bool> patternTable);

	private:
			static bool isBinaryValue(const Value &value);
			static Value getValueOrUnknown(const std::vector<Value> &values, const size_t &index);
			size_t calculateCaptureFFTCBetweenStates(const std::vector<Value> &fromValues, const std::vector<Value> &toValues, size_t *unknownPairs = NULL) const;
			size_t shiftScanState(std::vector<Value> &scanState, const Value &scanInValue, size_t *unknownPairs = NULL) const;
		};

	inline PatternProcessor::ShiftFFTCStats::ShiftFFTCStats()
			: patternCount_(0),
				scanFlipFlopCount_(0),
				shiftOperationCount_(0),
				shiftCycleCount_(0),
				totalShiftFFTC_(0),
				maxShiftFFTC_(0),
				totalUnknownShiftPairs_(0),
				averageShiftFFTC_(0.0)
	{
	}

	inline PatternProcessor::CaptureFFTCStats::CaptureFFTCStats()
			: patternCount_(0),
					scanFlipFlopCount_(0),
					captureCycleCount_(0),
					totalCaptureFFTC_(0),
					maxCaptureFFTC_(0),
					totalUnknownCapturePairs_(0),
					averageCaptureFFTC_(0.0)
		{
		}

	inline PatternProcessor::PatternProcessor()
	{
		staticCompression_ = OFF;
		dynamicCompression_ = OFF;
		XFill_ = OFF;
		XFillMode_ = XFILL_RANDOM;

		type_ = BASIC_SCAN;
		numPI_ = 0;
		numPPI_ = 0;
		numSI_ = 0;
		numPO_ = 0;
	}

	// **************************************************************************
	// Function   [ PatternProcessor::init ]
	// Commenter  [ CHT ]
	// Synopsis   [ usage: Basic setup initialization.
	//							description:
	// 								1. Set numPI_, numPO_ and numPPI_ from the Circuit.
	// 								2. Set pPIorder_, pPPIorder_ and pPOorder_ = {0, 1,..., numPI_-1}.
	// 								3. Increase each element in pPOorder_ by
	//									 (numGate_ of Circuit - numPO_ - numPPI_).
	// 								4. Increase each element in pPPIorder_ by numPPI_.
	//							argument:
	//								[in] pCircuit : The pointer to the target Circuit.
	//						]
	// Date       [ HKY Ver. 1.0 started 2014/09/01 last modified 2023/01/05 ]
	// **************************************************************************
	inline void PatternProcessor::init(Circuit *pCircuit)
	{
		pPIorder_.clear();
		pPPIorder_.clear();
		pPOorder_.clear();

		numPI_ = pCircuit->numPI_;
		numPO_ = pCircuit->numPO_;
		numPPI_ = pCircuit->numPPI_;

		pPIorder_.resize(numPI_);
		for (int i = 0; i < numPI_; ++i)
		{
			pPIorder_[i] = i;
		}

		pPOorder_.resize(numPO_);
		for (int i = 0; i < numPO_; ++i)
		{
			pPOorder_[i] = pCircuit->numGate_ - pCircuit->numPO_ - pCircuit->numPPI_ + i;
		}

		pPPIorder_.resize(numPPI_);
		for (int i = 0; i < numPPI_; ++i)
		{
			pPPIorder_[i] = pCircuit->numPI_ + i;
		}
	}

	inline bool PatternProcessor::isBinaryValue(const Value &value)
	{
		return value == L || value == H;
	}

	inline Value PatternProcessor::getValueOrUnknown(const std::vector<Value> &values, const size_t &index)
	{
		return index < values.size() ? values[index] : X;
	}

	inline size_t PatternProcessor::shiftScanState(std::vector<Value> &scanState, const Value &scanInValue, size_t *unknownPairs) const
	{
		if (unknownPairs)
		{
			*unknownPairs = 0;
		}
		if (scanState.empty())
		{
			return 0;
		}

		size_t fftc = 0;
		size_t unknown = 0;
		for (size_t i = 0; i < scanState.size(); ++i)
		{
			const Value nextValue = i == 0 ? scanInValue : scanState[i - 1];
			if (!isBinaryValue(scanState[i]) || !isBinaryValue(nextValue))
			{
				++unknown;
				++fftc;
				continue;
			}
			if (scanState[i] != nextValue)
			{
				++fftc;
			}
		}

		for (size_t i = scanState.size() - 1; i > 0; --i)
		{
			scanState[i] = scanState[i - 1];
		}
		scanState[0] = scanInValue;

		if (unknownPairs)
		{
			*unknownPairs = unknown;
		}
		return fftc;
	}

		inline size_t PatternProcessor::calculateCaptureFFTCBetweenStates(const std::vector<Value> &fromValues, const std::vector<Value> &toValues, size_t *unknownPairs) const
		{
			if (unknownPairs)
			{
				*unknownPairs = 0;
			}

			if (fromValues.empty() || toValues.empty() || numPPI_ <= 0)
			{
				return 0;
			}

			const size_t expectedPairs = static_cast<size_t>(numPPI_);
			size_t comparePairs = fromValues.size() < toValues.size() ? fromValues.size() : toValues.size();
			comparePairs = comparePairs < expectedPairs ? comparePairs : expectedPairs;

			size_t unknown = expectedPairs - comparePairs;
			size_t fftc = unknown;
			for (size_t i = 0; i < comparePairs; ++i)
			{
				const Value &launchValue = fromValues[i];
				const Value &captureValue = toValues[i];
				if (!isBinaryValue(launchValue) || !isBinaryValue(captureValue))
				{
					++unknown;
				++fftc;
				continue;
			}
			if (launchValue != captureValue)
			{
				++fftc;
			}
		}

		if (unknownPairs)
		{
			*unknownPairs = unknown;
			}
			return fftc;
		}

	inline PatternProcessor::CaptureFFTCStats PatternProcessor::calculateCaptureFFTCStats() const
	{
		CaptureFFTCStats stats;
		stats.patternCount_ = patternVector_.size();
		stats.scanFlipFlopCount_ = numPPI_ > 0 ? static_cast<size_t>(numPPI_) : 0;

		for (const Pattern &pattern : patternVector_)
		{
			if (pattern.PPI_.empty() || pattern.PPO_.empty() || stats.scanFlipFlopCount_ == 0)
			{
				continue;
			}

			size_t cycleUnknown = 0;
			size_t cycleFftc = 0;
			if (!pattern.PPO1_.empty())
			{
				cycleFftc = calculateCaptureFFTCBetweenStates(pattern.PPI_, pattern.PPO1_, &cycleUnknown);
				++stats.captureCycleCount_;
				stats.totalCaptureFFTC_ += cycleFftc;
				stats.totalUnknownCapturePairs_ += cycleUnknown;
				if (cycleFftc > stats.maxCaptureFFTC_)
				{
					stats.maxCaptureFFTC_ = cycleFftc;
				}

				cycleUnknown = 0;
				cycleFftc = calculateCaptureFFTCBetweenStates(pattern.PPO1_, pattern.PPO_, &cycleUnknown);
				++stats.captureCycleCount_;
				stats.totalCaptureFFTC_ += cycleFftc;
				stats.totalUnknownCapturePairs_ += cycleUnknown;
				if (cycleFftc > stats.maxCaptureFFTC_)
				{
					stats.maxCaptureFFTC_ = cycleFftc;
				}
			}
			else
			{
				cycleFftc = calculateCaptureFFTCBetweenStates(pattern.PPI_, pattern.PPO_, &cycleUnknown);
				++stats.captureCycleCount_;
				stats.totalCaptureFFTC_ += cycleFftc;
				stats.totalUnknownCapturePairs_ += cycleUnknown;
				if (cycleFftc > stats.maxCaptureFFTC_)
				{
					stats.maxCaptureFFTC_ = cycleFftc;
				}
			}
		}

		if (stats.captureCycleCount_ > 0)
		{
			stats.averageCaptureFFTC_ = static_cast<double>(stats.totalCaptureFFTC_) / static_cast<double>(stats.captureCycleCount_);
		}
		return stats;
	}

	inline PatternProcessor::ShiftFFTCStats PatternProcessor::calculateShiftFFTCStats(const bool includeFinalUnload) const
	{
		ShiftFFTCStats stats;
		stats.patternCount_ = patternVector_.size();
		stats.scanFlipFlopCount_ = numPPI_ > 0 ? static_cast<size_t>(numPPI_) : 0;

		if (stats.scanFlipFlopCount_ == 0)
		{
			return stats;
		}

		std::vector<Value> scanState(stats.scanFlipFlopCount_, L);
		bool hasShiftData = false;
		for (const Pattern &pattern : patternVector_)
		{
			if (pattern.PPI_.empty())
			{
				continue;
			}

			hasShiftData = true;
			++stats.shiftOperationCount_;
			for (size_t cycle = 0; cycle < stats.scanFlipFlopCount_; ++cycle)
			{
				const size_t ppiIndex = stats.scanFlipFlopCount_ - 1 - cycle;
				const Value scanInValue = getValueOrUnknown(pattern.PPI_, ppiIndex);
				size_t unknownPairs = 0;
				const size_t fftc = shiftScanState(scanState, scanInValue, &unknownPairs);
				++stats.shiftCycleCount_;
				stats.totalShiftFFTC_ += fftc;
				stats.totalUnknownShiftPairs_ += unknownPairs;
				if (fftc > stats.maxShiftFFTC_)
				{
					stats.maxShiftFFTC_ = fftc;
				}
			}

			for (size_t i = 0; i < stats.scanFlipFlopCount_; ++i)
			{
				scanState[i] = getValueOrUnknown(pattern.PPO_, i);
			}
		}

		if (includeFinalUnload && hasShiftData)
		{
			++stats.shiftOperationCount_;
			for (size_t cycle = 0; cycle < stats.scanFlipFlopCount_; ++cycle)
			{
				size_t unknownPairs = 0;
				const size_t fftc = shiftScanState(scanState, L, &unknownPairs);
				++stats.shiftCycleCount_;
				stats.totalShiftFFTC_ += fftc;
				stats.totalUnknownShiftPairs_ += unknownPairs;
				if (fftc > stats.maxShiftFFTC_)
				{
					stats.maxShiftFFTC_ = fftc;
				}
			}
		}

		if (stats.shiftCycleCount_ > 0)
		{
			stats.averageShiftFFTC_ = static_cast<double>(stats.totalShiftFFTC_) / static_cast<double>(stats.shiftCycleCount_);
		}
		return stats;
	}

	// **************************************************************************
	// Function   [ PatternProcessor::StaticCompression ]
	// Commenter  [ HKY, CYW, CHT ]
	// Synopsis   [ usage: Do static compression.
	//							description:
	//								Compare each pair of the patterns and check
	// 								whether they are compatible.
	// 								(i.e. can be merged without value assignment conflict)
	// 								If so, merge these patterns bit by bit.
	// 								The rule of compression: (X,L) -> L, (X,H) -> H
	//						]
	// Date       [ HKY Ver. 1.0 started 2014/09/01 last modified 2023/01/05 ]
	// **************************************************************************
	inline void PatternProcessor::StaticCompression()
	{
		const size_t &size = patternVector_.size();
		std::vector<bool> mergeRecord(size, false);
		// for (size_t i = 0; i < size; ++i)
		// {
		// 	mergeRecord[i] = false;
		// }
		std::vector<bool> patternTable(size * size);
		for (size_t i = 0; i < size; ++i)
		{
			patternTable[i * size + i] = true;
		}
		// build pattern table for each pair of patterns
		for (size_t i = 0; i < size - 1; ++i)
		{
			if (mergeRecord[i] == true)
			{
				continue; // If the pattern has been merged before, no need to merge again
			}
			for (size_t j = i + 1; j < size; ++j)
			{
				if (mergeRecord[j] == true)
				{
					continue;
				}

				bool compatible = true; // Can be merged into one
				for (int k = 0; k < numPI_; ++k)
				{ // If any bit of the patterns has different values(one is high and one is low), the patterns are not compatible
					if (((patternVector_[i].PI1_[k] == L) && (patternVector_[j].PI1_[k] == H)) || ((patternVector_[i].PI1_[k] == H) && (patternVector_[j].PI1_[k] == L)))
					{
						compatible = false;
						break;
					}
				}
				if (!(patternVector_[i].PI2_.empty()) && (compatible == true))
				{ // If the pattern has second primary input, need to check it too
					for (int k = 0; k < numPI_; ++k)
					{
						if (((patternVector_[i].PI2_[k] == L) && (patternVector_[j].PI2_[k] == H)) || ((patternVector_[i].PI2_[k] == H) && (patternVector_[j].PI2_[k] == L)))
						{
							compatible = false;
							break;
						}
					}
				}
				if (!(patternVector_[i].PPI_.empty()) && (compatible == true))
				{ // Check ppi
					for (int k = 0; k < numPPI_; ++k)
					{
						if (((patternVector_[i].PPI_[k] == L) && (patternVector_[j].PPI_[k] == H)) || ((patternVector_[i].PPI_[k] == H) && (patternVector_[j].PPI_[k] == L)))
						{
							compatible = false;
							break;
						}
					}
				}
				if (!(patternVector_[i].SI_.empty()) && (compatible == true))
				{ // Check si
					for (int k = 0; k < numSI_; ++k)
					{
						if (((patternVector_[i].SI_[k] == L) && (patternVector_[j].SI_[k] == H)) || ((patternVector_[i].SI_[k] == H) && (patternVector_[j].SI_[k] == L)))
						{
							compatible = false;
							break;
						}
					}
				}
				// the patterns are compatible
				if (compatible == true)
				{
					patternTable[i * size + j] = true;
					patternTable[j * size + i] = true;
				}
				else
				{
					patternTable[i * size + j] = false;
					patternTable[j * size + i] = false;
				}
			}
		}

		// for each pair of patterns, try to merge them
		updateTable(mergeRecord, patternTable);
		// replace Pattern
		std::vector<Pattern> compPattern;
		for (size_t i = 0; i < patternVector_.size(); ++i)
		{
			if (mergeRecord[i] == false)
			{
				compPattern.push_back(patternVector_[i]);
			}
		}
		patternVector_ = compPattern;
	}
	// **************************************************************************
	// Function   [ PatternProcessor::updateTable ]
	// Commenter  [ CHT ]
	// Synopsis   [ usage: Function called in StaticCompression().
	//										 Try merging patterns according to the information
	//										 given in the two input arguments.
	//							description:
	//										 First stores each pair of compatible patterns,
	//										 and calculate their similarity.
	//										 If no patterns can be merged, break and return false.
	//										 Each time try merging the pairs with max similarity
	//										 and update the mergeRecord and patternTable.
	//										 Repeat the procedure until no remaining candidates.
	//							arguments:
	// 								[in] mergeRecord : Stores whether each pattern can be merged.
	// 								[in] patternTable : Stores whether each pair of patterns can
	//																		be merged.
	// 								[out] bool : true if patterns merged successfully;
	//														 false if patterns can't be compressed correctly.
	//						]
	// Date       [ CHT started 2023/01/05 ]
	// **************************************************************************
	inline bool PatternProcessor::updateTable(std::vector<bool> mergeRecord, std::vector<bool> patternTable)
	{
		const size_t &size = patternVector_.size();
		typedef std::pair<int, int> Pattern_pair;
		typedef std::map<Pattern_pair, int> Pattern_table;
		Pattern_table patternCandidate;
		// find pattern pair candidate
		for (size_t i = 0; i < size - 1; ++i)
		{
			for (size_t j = i + 1; j < size; ++j)
			{
				if (patternTable[i * size + j])
				{
					Pattern_pair temp_pair(i, j);
					patternCandidate[temp_pair] = 0;
				}
			}
		}
		// update similarity pattern pair candidate
		for (Pattern_table::iterator it = patternCandidate.begin(); it != patternCandidate.end(); ++it)
		{
			int i = it->first.first;
			int j = it->first.second;
			int similararityCount = 0;
			for (size_t k = 0; k < size; ++k)
			{
				if (patternTable[i * size + k] && patternTable[i * size + k] == patternTable[j * size + k])
				{
					++similararityCount;
				}
			}
			it->second = similararityCount;
		}

		while (!patternCandidate.empty())
		{
			std::vector<bool> updateSimilarityPattern(size, false);
			// for (size_t i = 0; i < size; ++i)
			// {
			// 	updateSimilarityPattern[i] = false;
			// }
			int similarityPattern_first = 0;
			int similarityPattern_second = 0;
			int maxSimilarity = 0;
			for (Pattern_table::iterator it = patternCandidate.begin(); it != patternCandidate.end(); ++it)
			{
				if (it->second > maxSimilarity)
				{
					maxSimilarity = it->second;
					similarityPattern_first = it->first.first;
					similarityPattern_second = it->first.second;
				}
			}
			if (maxSimilarity == 0)
			{
				return false;
			}

			// delete first pattern
			std::vector<int> del_first_pattern;
			if (maxSimilarity != 0)
			{
				size_t i = similarityPattern_first;
				size_t j = similarityPattern_second;
				for (size_t k = 0; k < size; ++k)
				{
					if (mergeRecord[k])
					{
						continue;
					}
					if (patternTable[i * size + k] == 1 && patternTable[j * size + k] == 0)
					{
						del_first_pattern.push_back(k);
					}
					updateSimilarityPattern[k] = patternTable[i * size + k] || patternTable[j * size + k];
					patternTable[i * size + k] = patternTable[i * size + k] && patternTable[j * size + k];
					patternTable[k * size + i] = patternTable[i * size + k];
				}
			}
			const size_t &del_first_pattern_size = del_first_pattern.size();
			for (size_t i = 0; i < del_first_pattern_size; ++i)
			{
				if (del_first_pattern[i] < similarityPattern_first)
				{
					Pattern_pair temp_pair(del_first_pattern[i], similarityPattern_first);
					patternCandidate.erase(temp_pair);
				}
				else
				{
					Pattern_pair temp_pair(similarityPattern_first, del_first_pattern[i]);
					patternCandidate.erase(temp_pair);
				}
			}
			std::vector<int> del_second_pattern;
			for (size_t i = 0; i < size; ++i)
			{
				if (mergeRecord[i])
				{
					continue;
				}

				if ((size_t)similarityPattern_second == i)
				{
					continue;
				}

				if (patternTable[similarityPattern_second * size + i])
				{
					del_second_pattern.push_back(i);
				}
			}
			const size_t &del_second_pattern_size = del_second_pattern.size();
			for (size_t i = 0; i < del_second_pattern_size; ++i)
			{
				if (del_second_pattern[i] < similarityPattern_second)
				{
					Pattern_pair temp_pair(del_second_pattern[i], similarityPattern_second);
					patternCandidate.erase(temp_pair);
				}
				else
				{
					Pattern_pair temp_pair(similarityPattern_second, del_second_pattern[i]);
					patternCandidate.erase(temp_pair);
				}
			}
			// merge patterns
			if (maxSimilarity != 0)
			{
				int i = similarityPattern_first;
				int j = similarityPattern_second;
				for (int k = 0; k < numPI_; ++k)
				{
					patternVector_[i].PI1_[k] = (patternVector_[i].PI1_[k] < patternVector_[j].PI1_[k]) ? patternVector_[i].PI1_[k] : patternVector_[j].PI1_[k];
				}
				if (!patternVector_[i].PI2_.empty())
				{
					for (int k = 0; k < numPI_; ++k)
					{
						patternVector_[i].PI2_[k] = (patternVector_[i].PI2_[k] < patternVector_[j].PI2_[k]) ? patternVector_[i].PI2_[k] : patternVector_[j].PI2_[k];
					}
				}
				if (!patternVector_[i].PPI_.empty())
				{
					for (int k = 0; k < numPPI_; ++k)
					{
						patternVector_[i].PPI_[k] = (patternVector_[i].PPI_[k] < patternVector_[j].PPI_[k]) ? patternVector_[i].PPI_[k] : patternVector_[j].PPI_[k];
					}
				}
				if (!patternVector_[i].SI_.empty())
				{
					for (int k = 0; k < numSI_; ++k)
					{
						patternVector_[i].SI_[k] = (patternVector_[i].SI_[k] < patternVector_[j].SI_[k]) ? patternVector_[i].SI_[k] : patternVector_[j].SI_[k];
					}
				}

				for (int k = 0; k < numPO_; ++k)
				{
					patternVector_[i].PO1_[k] = (patternVector_[i].PO1_[k] < patternVector_[j].PO1_[k]) ? patternVector_[i].PO1_[k] : patternVector_[j].PO1_[k];
				}
					if (!patternVector_[i].PO2_.empty())
					{
						for (int k = 0; k < numPO_; ++k)
					{
							patternVector_[i].PO2_[k] = (patternVector_[i].PO2_[k] < patternVector_[j].PO2_[k]) ? patternVector_[i].PO2_[k] : patternVector_[j].PO2_[k];
						}
					}
					if (!patternVector_[i].PPO1_.empty())
					{
						for (int k = 0; k < numPPI_; ++k)
						{
							patternVector_[i].PPO1_[k] = (patternVector_[i].PPO1_[k] < patternVector_[j].PPO1_[k]) ? patternVector_[i].PPO1_[k] : patternVector_[j].PPO1_[k];
						}
					}
					if (!patternVector_[i].PPO_.empty())
					{
						for (int k = 0; k < numPPI_; ++k)
					{
						patternVector_[i].PPO_[k] = (patternVector_[i].PPO_[k] < patternVector_[j].PPO_[k]) ? patternVector_[i].PPO_[k] : patternVector_[j].PPO_[k];
					}
				}
				mergeRecord[j] = true;
			}

			// update similarity pattern pair candidate
			for (size_t i = 0; i < size; ++i)
			{
				if (mergeRecord[i])
				{
					continue;
				}
				if (updateSimilarityPattern[i])
				{
					for (size_t j = 0; j < size; ++j)
					{
						if (i == j)
						{
							continue;
						}
						if (mergeRecord[j])
						{
							continue;
						}
						if (patternTable[i * size + j])
						{
							Pattern_pair temp_pair;
							if (i < j)
							{
								temp_pair.first = i;
								temp_pair.second = j;
							}
							else
							{
								temp_pair.first = j;
								temp_pair.second = i;
							}
							Pattern_table::iterator it = patternCandidate.find(temp_pair);
							if (it != patternCandidate.end())
							{
								int similararityCount = 0;
								for (size_t b = 0; b < size; ++b)
								{
									if (mergeRecord[b])
									{
										continue;
									}
									if (patternTable[i * size + b] && patternTable[i * size + b] == patternTable[j * size + b])
									{
										++similararityCount;
									}
								}
								it->second = similararityCount;
							}
						}
					}
				}
			}
		}
		return true;
	}
};

#endif
