// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2013, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Rene Rahn <rene.rahn@fu-berlin.de>
// ==========================================================================
// Implements a threading interface to efficiently thread a set of
// journaled sequences.
// ==========================================================================
#ifndef EXTRAS_INCLUDE_SEQAN_DATA_PARALLEL_DATA_PARALLEL_TRAVERSAL_H_
#define EXTRAS_INCLUDE_SEQAN_DATA_PARALLEL_DATA_PARALLEL_TRAVERSAL_H_

namespace seqan {

// ============================================================================
// Forwards
// ============================================================================

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

enum JstTraversalState
{
    JST_TRAVERSAL_STATE_NULL,
    JST_TRAVERSAL_STATE_MASTER,
    JST_TRAVERSAL_STATE_BRANCH
};

// TODO(rmaerker): Check if we can reduce this!
template <typename TProxy, typename TCoverage, typename TBranchNodeIterator, typename TCallerState>
struct BranchStackNode
{
    typedef TProxy TProxy_;
    typedef TCoverage TCoverage_;
    typedef ContainerView<TProxy_> TProxyView;
    typedef typename Iterator<TProxy_>::Type TProxyIterator_;

    // Helper structures to accurately handle branches.
    unsigned        _mappedHostPos;
    unsigned        _proxyId;
    int             _varSize;
    int             _insSize;

    // Properties of the current branch window.
    unsigned        _windowEndPosition;
    int             _beginOffset;
    TProxyIterator_ _windowDeltaBeginIt;
    TCoverage_      _branchCoverage;

    TBranchNodeIterator _contextBranchNode;
    TCallerState _callerState;

    BranchStackNode() : _mappedHostPos(0), _proxyId(0), _varSize(0), _insSize(0), _windowEndPosition(0), _beginOffset(0), _callerState()
    {}

    template <typename TProxyId, typename TPos, typename TVarSize, typename TInsSize, typename TOffset, typename TWindowSize>
    BranchStackNode(TProxyIterator_ & deltaBeginIt,
                    TProxyId proxyId,
                    TVarSize varSize,
                    TInsSize insSize,
                    TPos mappedEndPos,
                    TCoverage_ & branchCoverage,
                    TOffset offset,
                    TWindowSize const & windowSize) :
        _mappedHostPos(mappedEndPos),
        _proxyId(proxyId),
        _varSize(varSize),
        _insSize(insSize),
        _windowEndPosition(0),
        _branchCoverage(branchCoverage),
        _callerState()

    {
        _windowDeltaBeginIt = deltaBeginIt;
        _beginOffset = offset;
        _windowEndPosition = position(deltaBeginIt) + _insSize + windowSize;
    }

    ~BranchStackNode()
    {
        clear(_branchCoverage);
    }
};

struct TraverseStateMaster_;
typedef Tag<TraverseStateMaster_> StateTraverseMaster;

struct TraverseStateBranch_;
typedef Tag<TraverseStateBranch_> StateTraverseBranch;

template <typename TContainer, typename TState, typename TSpec>
class JstTraverser;

template <typename TDeltaMap, typename TTreeSpec, typename TState, typename TSpec>
class JstTraverser<JournaledStringTree<TDeltaMap, TTreeSpec>, TState, TSpec>
{
public:
    typedef JournaledStringTree<TDeltaMap, TTreeSpec> TContainer;
    typedef typename JournalData<TContainer>::Type TJournalSet;
    typedef typename Host<TJournalSet>::Type TReference;
    typedef typename Iterator<TReference, Rooted>::Type TMasterBranchIterator;

    typedef typename Value<TJournalSet>::Type TJournalString;
    typedef typename Iterator<TJournalString>::Type TJournalIterator;

    typedef typename Iterator<TDeltaMap, Rooted>::Type TBranchNodeIterator;
    typedef typename MappedCoverage<TDeltaMap>::Type TBitVector;

    typedef MergePointMap_<unsigned, TBitVector> TMergePointStore;  // TODO(rmaerker): Chage this.
    typedef JstBranchStack_<TContainer, TState> TBranchStack;

    typedef typename Size<TJournalSet>::Type TSize;
    typedef typename Position<TContainer>::Type TPosition;

    // Basics.
    JstTraversalState _traversalState;
    TContainer * _haystackPtr;  // Pointer to the underlying data parallel facade.

    // Sequence iterators.
    TMasterBranchIterator _masterIt;
    TMasterBranchIterator _masterItEnd;
    TJournalIterator _branchIt;

    // Coverage information.
    TBitVector _activeMasterCoverage;  // Active master coverage.
    TBitVector _delCoverage;  // Coverage of the recorded deletion.
    TBitVector _activeBranchCoverage;  // Active coverage of the branch.
//    TBitVector _delBranchCoverage;  // Probably don't need this.

    // Branch-node information.
    TBranchNodeIterator _branchNodeIt;
    TBranchNodeIterator _branchNodeItEnd;
    TBranchNodeIterator _proxyBranchNodeIt;
    TBranchNodeIterator _branchNodeInContextIt;  // Points to left node within context or behind the context.

    // Auxiliary structures.
    TMergePointStore _mergePointStack;  // Stores merge points, when deletions are connected to the master branch.
    TBranchStack     _branchStack;  // Handles the branches of the current tree.
    TSize _windowSize;
    bool _needInit;
    TState _lastMasterState;

    JstTraverser() : _traversalState(JST_TRAVERSAL_STATE_NULL),
                  _haystackPtr((TContainer*) 0),
                  _windowSize(1),
                  _needInit(true)
    {}

    JstTraverser(TContainer & haystack, TSize windowSize) :
        _traversalState(JST_TRAVERSAL_STATE_NULL),
        _windowSize(windowSize),
        _needInit(false)
    {
        init(*this, haystack);
    }
};

// ============================================================================
// Metafunctions
// ============================================================================

// ----------------------------------------------------------------------------
// Metafunction Container
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
struct Container<JstTraverser<TContainer, TState, TSpec> >
{
    typedef TContainer Type;
};

template <typename TContainer, typename TState, typename TSpec>
struct Container<JstTraverser<TContainer, TState, TSpec> const>
{
    typedef TContainer const Type;
};

// ----------------------------------------------------------------------------
// Metafunction Position
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
struct Position<JstTraverser<TContainer, TState, TSpec> >
{
    typedef typename JournalData<TContainer>::Type TJournalSet_;
    typedef typename Value<TJournalSet_>::Type TJournalString_;
    typedef typename Position<TJournalString_>::Type TPosition_;
    typedef Pair<TPosition_, TPosition_> TValue_;
    typedef String<TValue_> Type;
};

template <typename TContainer, typename TState, typename TSpec>
struct Position<JstTraverser<TContainer, TState, TSpec> const> :
    Position<JstTraverser<TContainer, TState, TSpec> >{};

// ----------------------------------------------------------------------------
// Metafunction Size
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
struct Size<JstTraverser<TContainer, TState, TSpec> > :
       Size<TContainer>{};

template <typename TContainer, typename TState, typename TSpec>
struct Size<JstTraverser<TContainer, TState, TSpec> const> :
       Size<TContainer const>{};

// ----------------------------------------------------------------------------
// Metafunction BranchNode
// ----------------------------------------------------------------------------

template <typename TJstTraverser>
struct BranchNode;

template <typename TContainer, typename TState, typename TSpec>
struct BranchNode<JstTraverser<TContainer, TState, TSpec> >
{
    typedef typename VariantData<TContainer>::Type TDeltaMap_;
    typedef typename Iterator<TDeltaMap_, Rooted>::Type Type;
};

template <typename TContainer, typename TState, typename TSpec>
struct BranchNode<JstTraverser<TContainer, TState, TSpec> const>
{
    typedef typename VariantData<TContainer>::Type TDeltaMap_;
    typedef typename Iterator<TDeltaMap_ const, Rooted>::Type Type;
};

// ============================================================================
// Functions
// ============================================================================

// ----------------------------------------------------------------------------
// Function _windowBeginPosition()                        [StateTraverseMaster]
// ----------------------------------------------------------------------------

// TODO(rmaerker): Make private
template <typename TContainer, typename TState, typename TContextPos, typename TRequireFullContext>
inline typename MakeSigned<typename Position<TContainer>::Type>::Type
_windowBeginPosition(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPos, TRequireFullContext> > const & traverser,
                    StateTraverseMaster const & /*tag*/)
{
    typedef typename Position<TContainer>::Type TPosition;
    typedef typename MakeSigned<TPosition>::Type TPos_;

    if (IsSameType<TContextPos, ContextPositionLeft>::VALUE)
        return position(traverser._masterIt);
    else
        return static_cast<TPos_>(position(traverser._masterIt)) - static_cast<TPos_>(traverser._windowSize - 1);
}

// ----------------------------------------------------------------------------
// Function _windowBeginPosition()                        [StateTraverseBranch]
// ----------------------------------------------------------------------------

// TODO(rmaerker): Make private
template <typename TContainer, typename TState, typename TContextPos, typename TRequireFullContext>
inline typename MakeSigned<typename Position<TContainer>::Type>::Type
_windowBeginPosition(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPos, TRequireFullContext> > const & traverser,
                     StateTraverseBranch const & /*tag*/)
{
    typedef typename Position<TContainer>::Type TPosition;
    typedef typename MakeSigned<TPosition>::Type TPos_;

    if (IsSameType<TContextPos, ContextPositionLeft>::VALUE)
        return position(traverser._branchIt);
    else
        return static_cast<TPos_>(position(traverser._branchIt)) - static_cast<TPos_>(traverser._windowSize - 1);
}

// ----------------------------------------------------------------------------
// Function windowBeginPosition()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline typename Position<TContainer>::Type
windowBeginPosition(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    if (isMaster(traverser))
        return _windowBeginPosition(traverser, StateTraverseMaster());
    else
        return _windowBeginPosition(traverser, StateTraverseBranch());
}

// ----------------------------------------------------------------------------
// Function clippedWindowBeginPosition()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec, typename TTraversalState>
inline typename Position<TContainer>::Type
clippedWindowBeginPosition(JstTraverser<TContainer, TState, TSpec> const & traverser,
                           TTraversalState const & /*tag*/)
{
    return _max(0, _windowBeginPosition(traverser, TTraversalState()));
}

// ----------------------------------------------------------------------------
// Function _windowEndPosition()                          [StateTraverseMaster]
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPos, typename TRequireFullContext>
inline typename Position<TContainer>::Type
_windowEndPosition(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPos, TRequireFullContext> > const & traverser,
                   StateTraverseMaster const & /*tag*/)
{
    if (IsSameType<TContextPos, ContextPositionLeft>::VALUE)
        return position(traverser._masterIt) + (traverser._windowSize - 1);
    else
        return position(traverser._masterIt);
}

// ----------------------------------------------------------------------------
// Function _windowEndPosition()                          [StateTraverseBranch]
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPos, typename TRequireFullContext>
inline typename Position<TContainer>::Type
_windowEndPosition(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPos, TRequireFullContext> > const & traverser,
                   StateTraverseBranch const & /*tag*/)
{
    if (IsSameType<TContextPos, ContextPositionLeft>::VALUE)
        return position(traverser._branchIt) + (traverser._windowSize - 1);
    else
        return position(traverser._branchIt);
}

// ----------------------------------------------------------------------------
// Function windowEndPosition()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline typename Position<TContainer>::Type
windowEndPosition(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    if (isMaster(traverser))
        return _windowEndPosition(traverser, StateTraverseMaster());
    else
        return _windowEndPosition(traverser, StateTraverseBranch());
}

// ----------------------------------------------------------------------------
// Function clippedWindowEndPosition()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec, typename TTraversalState>
inline typename Position<TContainer>::Type
clippedWindowEndPosition(JstTraverser<TContainer, TState, TSpec> const & traverser,
                         TTraversalState const & /*tag*/)
{
    if (IsSameType<TTraversalState, StateTraverseMaster>::VALUE)
        return _min(length(host(container(traverser))), _windowEndPosition(traverser, TTraversalState()));
    else
        return _min(length(*traverser._branchIt._journalStringPtr), _windowEndPosition(traverser, TTraversalState()));
}


// ----------------------------------------------------------------------------
// Function windowBegin()                                 [StateTraverseMaster]
// ----------------------------------------------------------------------------

// ContextPositionLeft.
template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> >::TMasterBranchIterator
windowBegin(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> > const & traverser,
            StateTraverseMaster const & /*tag*/)
{
    return traverser._masterIt;
}

// ContextPositionRight.
template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> >::TMasterBranchIterator
windowBegin(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> > const & traverser,
            StateTraverseMaster const & /*tag*/)
{
    return traverser._masterIt - (traverser._windowSize - 1);
}

// ----------------------------------------------------------------------------
// Function windowEnd()                                   [StateTraverseMaster]
// ----------------------------------------------------------------------------

// ContextPositionLeft.
template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> >::TMasterBranchIterator
windowEnd(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> > const & traverser,
          StateTraverseMaster const & /*tag*/)
{
    return traverser._masterIt + (traverser._windowSize - 1);
}

// ContextPositionRight.
template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> >::TMasterBranchIterator
windowEnd(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> > const & traverser,
          StateTraverseMaster const & /*tag*/)
{
    return traverser._masterIt;
}

// ----------------------------------------------------------------------------
// Function windowBegin()                                 [StateTraverseBranch]
// ----------------------------------------------------------------------------

// ContextPositionLeft.
template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> >::TJournalIterator
windowBegin(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> > const & traverser,
            StateTraverseBranch const & /*tag*/)
{
    return traverser._branchIt;
}

// ContextPositionRight.
template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> >::TJournalIterator
windowBegin(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> > const & traverser,
            StateTraverseBranch const & /*tag*/)
{
    return traverser._branchIt - (traverser._windowSize - 1);
}

// ----------------------------------------------------------------------------
// Function windowEnd()                                   [StateTraverseBranch]
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> >::TJournalIterator
windowEnd(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionLeft, TContextBegin> > const & traverser,
          StateTraverseBranch const & /*tag*/)
{
    return traverser._branchIt + (traverser._windowSize - 1);
}

template <typename TContainer, typename TState, typename TContextBegin>
inline typename JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> >::TJournalIterator
windowEnd(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, TContextBegin> > const & traverser,
          StateTraverseBranch const & /*tag*/)
{
    return traverser._branchIt;
}

// ----------------------------------------------------------------------------
// Function refresh()
// ----------------------------------------------------------------------------

// TODO(rmaerker): Depreacted - Remove!
//template <typename TContainer, typename TState, typename TSpec, typename TOperator, typename TDelegate>
//inline void
//refresh(JstTraverser<TContainer, TState, TSpec> & traverser,
//        TOperator & traversalCaller,
//        TDelegate & delegate,
//        StateTraverseMaster const & /*tag*/)
//{
////    typedef JstTraverser<TContainer, TSpec> TJstTraverser;
////    typedef typename ComputeState<TJstTraverser>::Type TComputeState;
//
////    TComputeState res = compute(traversalCaller, traverser._masterIt);
////    if (res.i1)
////    {
////        // Check if we passed some merge points until here.
////        _syncAndUpdateCoverage(traverser, StateTraverseMaster());
//////        _syncToMergePoint(traverser._activeMasterCoverage, traverser._mergePointStack,
//////                          position(windowBegin(traverser, StateTraverseMaster())),
//////                          MergePointSyncResize());
////        delegate(traverser);  // Calls the option that should be executed if the response evaluates to true.
////    }
//    traverser._masterIt += deliverContext(traversalCaller, delegate, traverser);
//}

// TODO(rmaerker): Deprecated! Remove!
//template <typename TContainer, typename TState, typename TSpec, typename TOperator, typename TDelegate, typename TBranchStackNode>
//inline typename Size<JstTraverser<TContainer, TState, TSpec> >::Type
//refresh(JstTraverser<TContainer, TState, TSpec> & traverser,
//        TOperator & traversalCaller,
//        TDelegate & delegate,
//        TBranchStackNode & branchStackNode,
//        StateTraverseBranch const & /*tag*/)
//{
//    typedef JstTraverser<TContainer, TSpec> TJstTraverser;
//    typedef typename Size<TContainer>::Type TSize;
//
//    TSize shiftLength = deliverContext(traversalCaller, delegate, traverser);
////    if (res.i1)
////    {
////        traverser._activeBranchCoverage = branchStackNode._branchCoverage;
////        _syncAndUpdateCoverage(traverser, branchStackNode, StateTraverseBranch());
////        delegate(traverser);  // Calls the option that should be executed if the response evaluates to true.
////    }
//    traverser._branchIt += shiftLength;
//    return shiftLength;
//}

// ----------------------------------------------------------------------------
// Function _globalInit()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline void
_globalInit(JstTraverser<TContainer, TState, TSpec> & traverser)
{
    typedef JstTraverser<TContainer, TState, TSpec> TJstTraverser;
    typedef typename VariantData<TContainer>::Type TDeltaMap;
    typedef typename MappedDelta<TDeltaMap>::Type TMappedDelta;
    typedef typename MappedCoverage<TDeltaMap>::Type TMappedCoverage;
    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIt;

    resize(traverser._activeMasterCoverage, journalSize(container(traverser)), true, Exact());  // Set all seq active.
    traverser._delCoverage = traverser._activeMasterCoverage;  // Set the current master coverage.
//    traverser._delBranchCoverage = traverser._delCoverage;  // Set the current master coverage.

//    init(traverser._vpManager, journalSize(container(traverser)));
    // NOTE(rmaerker): We add a dummy value to the stack, such that it is never empty.
    push(traverser._mergePointStack, length(host(container(traverser))) + 1, traverser._activeMasterCoverage);

    // TODO(rmaerker): We probably don't need this for initialization.
    if (_windowEndPosition(traverser, StateTraverseMaster()) < *traverser._branchNodeIt)
    {
        traverser._traversalState = JST_TRAVERSAL_STATE_MASTER;
    }
    else
    {  // We set the active branch state to the delta branch.
        TBranchNodeIt tmpIt = traverser._branchNodeIt;
        while(tmpIt != traverser._branchNodeItEnd && *tmpIt == 0)
        {
            TMappedDelta res = mappedDelta(variantData(container(traverser)), position(traverser._branchNodeIt));
            // TODO(rmaerker): Add case for INDEL
            if (deltaType(res) != DeltaType::DELTA_TYPE_DEL)
            {
                ++tmpIt;
                traverser._traversalState = JST_TRAVERSAL_STATE_BRANCH;
                continue;
            }

            // If there is a deletion at the beginning, update the active master coverage.
            TMappedCoverage &varCoverage = mappedCoverage(variantData(container(traverser)), position(tmpIt));
            traverser._delCoverage = ~varCoverage;
//            bitwiseAndNot(traverser._delCoverage, traverser._delCoverage, varCoverage);
            push(traverser._mergePointStack, deltaDel(variantData(container(traverser)), deltaPosition(res)),
                 varCoverage);
            ++tmpIt;
        }
    }
}

// ----------------------------------------------------------------------------
// Function position()
// ----------------------------------------------------------------------------

//template <typename TContainer, typename TSpec>
//inline typename Position<JstTraverser<JournaledStringTree<TContainer, StringTreeSparse>, TSpec> const >::Type
//_position(JstTraverser<JournaledStringTree<TContainer, StringTreeSparse>, TSpec> const & traverser)
//{
//    typedef JournaledStringTree<TContainer, StringTreeDefault> TStringTree;
//    typedef JstTraverser<TStringTree, TSpec> const TJstTraverser;
//    typedef typename Position<TJstTraverser>::Type TPosition;
//    typedef typename Value<TPosition>::Type TPositionValue;
//
//    typedef typename VariantData<TStringTree>::Type TDeltaMap;
//    typedef typename MappedCoverage<TDeltaMap>::Type TCoverage;
//    typedef typename Iterator<TCoverage const, Standard>::Type TIterator;
//
//    TPosition posVec;
//    TIterator itBegin;
//    TIterator itEnd;
//    if (traverser._traversalState == JST_TRAVERSAL_STATE_MASTER)
//    {
//        itBegin = begin(traverser._activeMasterCoverage, Standard());
//        itEnd = end(traverser._activeMasterCoverage, Standard());
//    }
//    else
//    {
//        SEQAN_ASSERT_EQ(traverser._traversalState, JST_TRAVERSAL_STATE_BRANCH);
//        itBegin = begin(traverser._activeBranchCoverage, Standard());
//        itEnd = end(traverser._activeBranchCoverage, Standard());
//    }
//    // Return the virtual position of all selected sequences.
//    for (TIterator it = itBegin; it != itEnd; ++it)
//    {
//        if (*it)
//        {
//            unsigned seqId = it - itBegin;
//            appendValue(posVec, TPositionValue(seqId, value(traverser._vpManager, seqId)));
//        }
//    }
//    return posVec;
//}

// ----------------------------------------------------------------------------
// Function position()
// ----------------------------------------------------------------------------

// TODO(rmaerker): Adapt to new interface.
template <typename TContainer, typename TState, typename TContextPos, typename TContextBegin>
inline typename Position<JstTraverser<JournaledStringTree<TContainer, StringTreeDefault>, TState, JstTraverserSpec<TContextPos, TContextBegin> > >::Type
position(JstTraverser<JournaledStringTree<TContainer, StringTreeDefault>, TState, JstTraverserSpec<TContextPos, TContextBegin> > & traverser)
{
    typedef JournaledStringTree<TContainer, StringTreeDefault> THaystack;
    typedef JstTraverser<THaystack, TState, JstTraverserSpec<TContextPos, TContextBegin> > TJstTraverser;
    typedef typename Position<TJstTraverser>::Type TPosition;
    typedef typename Value<TPosition>::Type TPositionValue;

    typedef typename JournalData<THaystack>::Type TJournalSet;
    typedef typename Value<TJournalSet>::Type TJournalString;
    typedef typename Iterator<TJournalString, Standard>::Type TJournalStringIt;
    typedef typename VariantData<THaystack>::Type TDeltaMap;
    typedef typename MappedCoverage<TDeltaMap>::Type TCoverage;
    typedef typename Iterator<TCoverage, Standard>::Type TIterator;

    TPosition posVec;

    // TODO(rmaerker): synchronize before.

    if (traverser._traversalState == JST_TRAVERSAL_STATE_MASTER)
    {  // Easy case, since all sequences must be in a original node.
        _syncAndUpdateCoverage(traverser, StateTraverseMaster());
        unsigned hostPos = position(traverser._masterIt); //position(windowBegin(traverser, StateTraverseMaster()));  // Current position within the host.
        TIterator itBegin = begin(traverser._activeMasterCoverage, Standard());
        TIterator itEnd = end(traverser._activeMasterCoverage, Standard());
        for (TIterator it = itBegin; it != itEnd; ++it)
        {
            if (*it)
            {
                unsigned seqId = it - itBegin;
                appendValue(posVec,
                            TPositionValue(seqId, hostToVirtualPosition(value(journalData(container(traverser)), seqId),
                                                                        hostPos)));
            }
        }
    }
    else
    {  // Harder case, the virtual positions cannot easily be rebased to a common host pos.
        SEQAN_ASSERT_EQ(traverser._traversalState, JST_TRAVERSAL_STATE_BRANCH);

        _syncAndUpdateCoverage(traverser, StateTraverseBranch());

        TIterator itBegin = begin(traverser._activeBranchCoverage, Standard());
        TIterator itEnd = end(traverser._activeBranchCoverage, Standard());
        if (IsSameType<TContextPos, ContextPositionRight>::VALUE)
        {
            // TODO(rmaerker): Usually we just take the current position branch iterator.
            int offset = 0; //position(traverser._branchIt) - windowBeginPositionClipped(traverser, StateTraverseBranch());
            for (TIterator it = itBegin; it != itEnd; ++it)
            {
                if (*it)
                {
                    unsigned seqId = it - itBegin;
                    TJournalStringIt journalIt;
                    // This is ok, since we guarantee not to change anything in this function.
                    journalIt._journalStringPtr = const_cast<TJournalString*>(&value(journalData(container(traverser)), seqId));
                    // We now need to figure out a way to map the end positions!!!! Maybe we need some diner for this.
                    // Maybe but just maybe, we need the last break point here.
                    _mapVirtualToVirtual(journalIt, traverser._branchIt, (traverser._proxyBranchNodeIt - 1), variantData(container(traverser)), seqId);
                    appendValue(posVec, TPositionValue(seqId, position(journalIt) + offset));
                }
            }
        }
        else
        {
            for (TIterator it = itBegin; it != itEnd; ++it)
            {
                if (*it)
                {
                    unsigned seqId = it - itBegin;
                    TJournalStringIt journalIt;
                    // This is ok, since we guarantee not to change anything in this function.
                    journalIt._journalStringPtr = const_cast<TJournalString*>(&value(journalData(container(traverser)), seqId));
                    _mapVirtualToVirtual(journalIt, windowBegin(traverser, StateTraverseBranch()), traverser._branchNodeIt, variantData(container(traverser)), seqId);
                    appendValue(posVec, TPositionValue(seqId, position(journalIt)));
                }
            }
        }

    }
    return posVec;
}

// ----------------------------------------------------------------------------
// Function state()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline JstTraversalState
state(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    return traverser._traversalState;
}

// ----------------------------------------------------------------------------
// Function isMaster()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline bool
isMaster(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    return state(traverser) == JST_TRAVERSAL_STATE_MASTER;
}

// ----------------------------------------------------------------------------
// Function isBranch()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline bool
isBranch(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    return state(traverser) == JST_TRAVERSAL_STATE_BRANCH;
}

// ----------------------------------------------------------------------------
// Function position()
// ----------------------------------------------------------------------------

//template <typename TContainer, typename TState, typename TSpec>
//inline typename Position<JstTraverser<TContainer, TState, TSpec> const >::Type
//position(JstTraverser<TContainer, TState, TSpec> & traverser)
//{
//    return _position(traverser);
//}

// ----------------------------------------------------------------------------
// Function contextIterator()
// ----------------------------------------------------------------------------

// StateTraverseMaster.
template <typename TContainer, typename TState, typename TSpec>
inline typename JstTraverser<TContainer, TState, TSpec>::TMasterBranchIterator &
contextIterator(JstTraverser<TContainer, TState, TSpec> & traverser,
                StateTraverseMaster const & /*tag*/)
{
    return traverser._masterIt;
}

template <typename TContainer, typename TState, typename TSpec>
inline typename JstTraverser<TContainer, TState, TSpec>::TMasterBranchIterator const &
contextIterator(JstTraverser<TContainer, TState, TSpec> const & traverser,
                StateTraverseMaster const & /*tag*/)
{
    return traverser._masterIt;
}

// StateTraverseBranch.
template <typename TContainer, typename TState, typename TSpec>
inline typename JstTraverser<TContainer, TState, TSpec>::TJournalIterator &
contextIterator(JstTraverser<TContainer, TState, TSpec> & traverser,
                StateTraverseBranch const & /*tag*/)
{
    return traverser._branchIt;
}

template <typename TContainer, typename TState, typename TSpec>
inline typename JstTraverser<TContainer, TState, TSpec>::TJournalIterator const &
contextIterator(JstTraverser<TContainer, TState, TSpec> const & traverser,
                StateTraverseBranch const & /*tag*/)
{
    return traverser._branchIt;
}

// ----------------------------------------------------------------------------
// Function coverage()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline typename MappedCoverage<typename VariantData<TContainer const>::Type> ::Type &
coverage(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    if (isMaster(traverser))
        return traverser._activeMasterCoverage;
    return traverser._activeBranchCoverage;
}

template <typename TContainer, typename TState, typename TSpec>
inline typename MappedCoverage<typename VariantData<TContainer>::Type> ::Type &
coverage(JstTraverser<TContainer, TState, TSpec> & traverser)
{
    if (isMaster(traverser))
        return traverser._activeMasterCoverage;
    return traverser._activeBranchCoverage;
}

// ----------------------------------------------------------------------------
// Function branchNode()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline typename BranchNode<JstTraverser<TContainer, TState, TSpec> >::Type &
branchNode(JstTraverser<TContainer, TState, TSpec> & traverser)
{
    return traverser._branchNodeIt;
}

template <typename TContainer, typename TState, typename TSpec>
inline typename BranchNode<JstTraverser<TContainer, TState, TSpec> const>::Type &
branchNode(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    return traverser._branchNodeIt;
}

// ----------------------------------------------------------------------------
// Function _selectNextSplitPoint()
// ----------------------------------------------------------------------------

template <typename TBranchStackNode, typename TNodeIt>
inline unsigned
_selectNextSplitPoint(TBranchStackNode const & proxyWindow,
                      TNodeIt const & nodeIt,
                      TNodeIt const & branchNode)
{
    int virtualMapping = (*nodeIt - *branchNode);// - patternLength;
    unsigned splitPointPos = position(proxyWindow._proxyIter) + virtualMapping - proxyWindow._proxyEndPosDiff;
//    unsigned splitPointPos = position(proxyWindow._proxyIter) + virtualMapping - proxyWindow._varSize + proxyWindow._insSize;

#ifdef DEBUG_DATA_PARALLEL
    std::cerr << "Virtual Positions: " << position(proxyWindow._proxyIter) << " to " << splitPointPos <<  std::endl;
    std::cerr << "Physical Positions: " << *branchNode << " to " << *nodeIt << std::endl;
#endif
    return splitPointPos;
}

// ----------------------------------------------------------------------------
// Function _updateAuxiliaryBranchStructures()
// ----------------------------------------------------------------------------

template <typename TBranchStackEntry, typename TTraverser, typename TMappedKey>
inline void
_updateAuxiliaryBranchStructures(TBranchStackEntry & branchEntry,
                                 TTraverser & traverser,
                                 TMappedKey & varKey)
{
    if (deltaType(varKey) == DeltaType::DELTA_TYPE_DEL)
    {
        branchEntry._proxyEndPosDiff += deltaDel(variantData(container(traverser)), deltaPosition(varKey));
        branchEntry._mappedHostPos += deltaDel(variantData(container(traverser)), deltaPosition(varKey)) - 1;
//        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < branchEntry._mappedHostPos)
//            ++traverser._proxyBranchNodeIt;
    }
    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INS)
    {
        branchEntry._proxyEndPosDiff -= static_cast<int>(length(deltaIns(variantData(container(traverser)), deltaPosition(varKey))));
    }
    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INDEL)
    {
        branchEntry._proxyEndPosDiff += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
        branchEntry._mappedHostPos += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < branchEntry._mappedHostPos)
//            ++traverser._proxyBranchNodeIt;
        branchEntry._proxyEndPosDiff -= static_cast<int>(length(deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i2));
    }
}

// ----------------------------------------------------------------------------
// Function _traverseBranch()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPosition, typename TRequireFullContext,
          typename TExternal, typename TDelegate>
inline void
_traverseBranch(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPosition, TRequireFullContext> > & traverser,
                TExternal & externalAlg,
                TDelegate & delegate)
{
    typedef JstTraverserSpec<TContextPosition, TRequireFullContext> TJstTraverserSpec_;
    typedef JstTraverser<TContainer, TState, TJstTraverserSpec_ > TJstTraverser;
    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIter;

    typedef typename TJstTraverser::TBranchStack TBranchStack;
    typedef typename Value<TBranchStack>::Type TBranchStackEntry;

    typedef typename VariantData<TContainer>::Type TVariantMap;
    typedef typename MappedCoverage<TVariantMap>::Type TBitVector;
    typedef typename MappedDelta<TVariantMap>::Type TMappedDelta;

    TBranchStackEntry & deltaWindow = top(traverser._branchStack);

#ifdef DEBUG_DATA_PARALLEL
        std::cerr << "o Search Branch: " <<  deltaWindow._branchProxyId << std::endl;
        std::cerr << "Virtual Space: " << position(deltaWindow._proxyIter) - deltaWindow._prefixOffset << " - " << deltaWindow._prefixEndPos << std::endl;
        std::cerr << "Break Point: " << position(deltaWindow._proxyIter) << std::endl;
        std::cerr << "Coverage: " << traverser._activeBranchCoverage << std::endl;
#endif

//    setState(externalAlg, deltaWindow._callerState);  // Set the last active state before the branch.

    // Select the next node.
    traverser._proxyBranchNodeIt = traverser._branchNodeIt;
    TBranchNodeIter nodeItEnd = traverser._branchNodeItEnd -1;  // TODO(rmaerker): Check if we can put it on the register.

    unsigned splitPointPos = 0;
    if (traverser._proxyBranchNodeIt != nodeItEnd)
    {
        // Move right until first node whose host pos is equal or greater than the current mappedHostPos.
        while(traverser._proxyBranchNodeIt != nodeItEnd && *traverser._proxyBranchNodeIt < deltaWindow._mappedHostPos)
            ++traverser._proxyBranchNodeIt;
        if (*traverser._proxyBranchNodeIt < deltaWindow._mappedHostPos)
        {
            splitPointPos = deltaWindow._proxyEndPos;
            ++traverser._proxyBranchNodeIt; // Set to the end.
        }
        else
            splitPointPos = _selectNextSplitPoint(deltaWindow, traverser._proxyBranchNodeIt, traverser._branchNodeIt);
    }
    else
    {
        splitPointPos = deltaWindow._proxyEndPos;
        ++traverser._proxyBranchNodeIt; // Set to the end.
    }

#ifdef DEBUG_DATA_PARALLEL
    std::cerr << "split point: " << splitPointPos << " ("<< *nodeIt << ")" << std::endl;
#endif

    // Set the correct iterator here.
    if (IsSameType<TContextPosition, ContextPositionLeft>::VALUE)
    {
        if (deltaWindow._prefixOffset < 0)
            traverser._branchIt = deltaWindow._proxyIter + -(deltaWindow._prefixOffset);
        else
            traverser._branchIt = deltaWindow._proxyIter - deltaWindow._prefixOffset;
    }
    else
    {
        traverser._branchIt = deltaWindow._proxyIter + ((traverser._windowSize -1) - deltaWindow._prefixOffset);
    };

    // Note, we need the position of the iterator, because the journal string iterator never
    // exceeds the end of the journal string. And it might be faster than evaluating the iterator.
    unsigned branchPos = _windowEndPosition(traverser, StateTraverseBranch());
    TBitVector splitVec;

    while(branchPos < deltaWindow._proxyEndPos && branchPos < length(*(deltaWindow._proxyIter._journalStringPtr)))  // Check end condition.
    {
        if (branchPos >= splitPointPos)
        {
            // First check if we need to update the positions first.
            // Get the variant coverage for the split point.
            TBitVector & varCoverage = mappedCoverage(variantData(container(traverser)), position(traverser._proxyBranchNodeIt));
            transform(splitVec, varCoverage, deltaWindow._branchCoverage, FunctorBitwiseAnd());

            // Check if found split point effects the current branch.
            if (!testAllZeros(splitVec) && !_testEqual(splitVec, deltaWindow._branchCoverage))
            {

                // Appending to the branch stack might invalidate the current pointer.
                TBranchStackEntry & splitProxyWindow = createEntry(traverser._branchStack);

                splitProxyWindow._mappedHostPos = *traverser._proxyBranchNodeIt;  // The host position of the split point, where we spawn the search tree.
                splitProxyWindow._proxyEndPosDiff = deltaWindow._proxyEndPosDiff;
                splitProxyWindow._externalState = getState(externalAlg); // Current state before the split.
                splitProxyWindow._firstWindowBranchNode = deltaWindow._firstWindowBranchNode;  // TODO(rmaerker): Do we need this?

                splitProxyWindow._prefixOffset = static_cast<int>(position(deltaWindow._proxyIter)) -
                                                 static_cast<int>(_windowBeginPosition(traverser, StateTraverseBranch()));
                splitProxyWindow._proxyIter = deltaWindow._proxyIter;

                // Check if current branch covers the new variant.
                if (splitVec[deltaWindow._branchProxyId]) //&proxySeq == deltaWindow._proxyView._begin._journalStringPtr)   // TODO(rmaerker): Check with: splitVec[window._proxyId] == true => current proxy shares the variant.
                {  // Split branch for all sequences that do not share the new delta.
                    // Set the coverage of the split branch.
                    transform(splitProxyWindow._branchCoverage, deltaWindow._branchCoverage, splitVec,
                              FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
                    // Update the coverage of the current branch.
                    deltaWindow._branchCoverage = splitVec;  // TODO(rmaerker): Use swap/move


                    // Update auxiliary branch structures.
                    // Check the type of the current variant.
//                    TMappedDelta varKey = mappedDelta(variantData(container(traverser)), position(traverser._proxyBranchNodeIt));
                    _updateAuxiliaryBranchStructures(deltaWindow, traverser,
                                                     mappedDelta(variantData(container(traverser)),
                                                                 position(traverser._proxyBranchNodeIt)));
                    // If current split point involves a deletion we need to update the correct host position.
//                    if (deltaType(varKey) == DeltaType::DELTA_TYPE_DEL)
//                    {
//                        deltaWindow._varSize += deltaDel(variantData(container(traverser)), deltaPosition(varKey));
//                        deltaWindow._mappedHostPos += deltaDel(variantData(container(traverser)), deltaPosition(varKey));
//                        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
//                            ++traverser._proxyBranchNodeIt;
//                    }
//                    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INS)
//                    {
//                        deltaWindow._insSize += length(deltaIns(variantData(container(traverser)), deltaPosition(varKey)));
//                    }
//                    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INDEL)
//                    {
//                        deltaWindow._varSize += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//                        deltaWindow._mappedHostPos += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//                        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
//                            ++traverser._proxyBranchNodeIt;
//                        deltaWindow._insSize += length(deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i2);
//                    }
                }
                else
                {
                    // Udpate the split branch coverage.
                    splitProxyWindow._branchCoverage = splitVec; // TODO(rmaerker): swap/move here.
                    // Update the current branch coverage.
                    transform(deltaWindow._branchCoverage, deltaWindow._branchCoverage, splitVec,
                              FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
                    // Get the type of the next variant.
//                    TMappedDelta varKey = mappedDelta(variantData(container(traverser)), position(traverser._proxyBranchNodeIt));
                    ++splitProxyWindow._mappedHostPos;
                    _updateAuxiliaryBranchStructures(splitProxyWindow, traverser,
                                                     mappedDelta(variantData(container(traverser)),
                                                                 position(traverser._proxyBranchNodeIt)));


                    // TODO(rmaerker): What happens if it points directly onto the deletion?
//                    if (deltaType(varKey) == DeltaType::DELTA_TYPE_DEL)
//                    {
//                        splitProxyWindow._varSize += deltaDel(variantData(container(traverser)), deltaPosition(varKey));
//                        splitProxyWindow._mappedHostPos += deltaDel(variantData(container(traverser)), deltaPosition(varKey)) -1;
//                        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
//                            ++traverser._proxyBranchNodeIt;
//                    }
//                    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INS)
//                    {
//                        splitProxyWindow._insSize += length(deltaIns(variantData(container(traverser)), deltaPosition(varKey)));
//                    }
//                    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INDEL)
//                    {
//                        splitProxyWindow._varSize += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//                        splitProxyWindow._mappedHostPos += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//                        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
//                            ++traverser._proxyBranchNodeIt;
//                        splitProxyWindow._insSize += length(deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i2);
//                    }
                    // TODO(rmaerker): Add case for INS_DEL!
                }

#ifdef DEBUG_DATA_PARALLEL
                std::cerr << "-> split branch proxy: " << splitProxyWindow._branchProxyId << std::endl;
                std::cerr << "-> split branch vp: " << position(splitProxyWindow._proxyIter) - splitProxyWindow._prefixOffset << " - " << splitProxyWindow._proxyEndPos<< std::endl;
                std::cerr << "-> Original branch point: " << position(splitProxyWindow._proxyIter) << std::endl;
#endif
            }
            else
            {
                if (mappedCoverage(variantData(container(traverser)), position(traverser._proxyBranchNodeIt))[deltaWindow._branchProxyId])
                {
//                    TMappedDelta varKey = mappedDelta(variantData(container(traverser)), position(traverser._proxyBranchNodeIt ));
                    _updateAuxiliaryBranchStructures(deltaWindow, traverser,
                                                     mappedDelta(variantData(container(traverser)),
                                                                 position(traverser._proxyBranchNodeIt)));


                    // If current split point involves a deletion we need to update the correct host position.
//                    if (deltaType(varKey) == DeltaType::DELTA_TYPE_DEL)
//                    {
//                        deltaWindow._varSize += deltaDel(variantData(container(traverser)), deltaPosition(varKey));
//                        deltaWindow._mappedHostPos += deltaDel(variantData(container(traverser)), deltaPosition(varKey));
//                        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
//                            ++traverser._proxyBranchNodeIt;
//                    }
//                    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INS)
//                    {
//                        deltaWindow._insSize += length(deltaIns(variantData(container(traverser)), deltaPosition(varKey)));
//                    }
//                    else if (deltaType(varKey) == DeltaType::DELTA_TYPE_INDEL)
//                    {
//                        deltaWindow._varSize += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//                        deltaWindow._mappedHostPos += deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i1;
//                        while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
//                            ++traverser._proxyBranchNodeIt;
//                        deltaWindow._insSize += length(deltaIndel(variantData(container(traverser)), deltaPosition(varKey)).i2);
//                    }
                }
            }
            TMappedDelta varKey = mappedDelta(variantData(container(traverser)), position(traverser._proxyBranchNodeIt ));
            if (deltaType(varKey) == DeltaType::DELTA_TYPE_DEL || deltaType(varKey) == DeltaType::DELTA_TYPE_INDEL)
                while(nodeItEnd != traverser._proxyBranchNodeIt && *(traverser._proxyBranchNodeIt + 1) < deltaWindow._mappedHostPos)
                    ++traverser._proxyBranchNodeIt;

            if (traverser._proxyBranchNodeIt != nodeItEnd)
                splitPointPos = _selectNextSplitPoint(deltaWindow, ++traverser._proxyBranchNodeIt, traverser._branchNodeIt);
            else
            {
                splitPointPos = deltaWindow._proxyEndPos;
                ++traverser._proxyBranchNodeIt;
            }

#ifdef DEBUG_DATA_PARALLEL
                std::cerr << "-> split branch split point: " << splitPointPos << " ("<< *traverser._proxyBranchNodeIt  << ")" << std::endl;
#endif
            continue;
        }
        unsigned shiftSize = deliverContext(externalAlg, delegate, traverser, StateTraverseBranch());
        branchPos += shiftSize;
        traverser._branchIt += shiftSize;
//        branchPos += refresh(traverser, externalAlg, delegate, deltaWindow, StateTraverseBranch());

#ifdef DEBUG_DATA_PARALLEL
        std::cerr << "--- position: " << position(windowBegin(traverser, StateTraverseBranch())) << std::endl;
#endif
    }
}

// ----------------------------------------------------------------------------
// Function _updateBranchBegin()
// ----------------------------------------------------------------------------

template <typename TProxyId, typename TJstTraverser, typename TPosition, typename TCoverage>
inline typename Size<typename Container<TJstTraverser>::Type>::Type
_selectValidBeginAndProxy(TProxyId & proxyId,
                          TJstTraverser & traverser,
                          TPosition const & contextBeginPosHost,
                          TCoverage const & branchCoverage)
{
    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIterator;

    typedef typename TJstTraverser::TMergePointStore TMergePointStack;
    typedef typename TMergePointStack::TMergePoints TMergePoints;
    typedef typename Iterator<TMergePoints>::Type TMergePointIterator;

    typedef typename Size<TJstTraverser>::Type TSize;

    TCoverage tmp;
    transform(tmp, branchCoverage, traverser._activeMasterCoverage, FunctorBitwiseAnd());
//    bitwiseAnd(tmp, branchCoverage, traverser._activeMasterCoverage);

    if (!testAllZeros(tmp))  // There exist a valid begin at the current mapped master position.
    {
        proxyId = bitScanForward(tmp);
        return *traverser._branchNodeIt - contextBeginPosHost;
    }

    // Harder case: We need to parse all possible positions to find the first valid proxy, which is furthest away
    // from the current branch point.

    // Initialization
    TCoverage seenVariants;
    resize(seenVariants, journalSize(container(traverser)), false, Exact());

    // The merge point iter is always valid.
    SEQAN_ASSERT_NOT(empty(traverser._mergePointStack._mergePoints));
    TMergePointIterator beginMp = begin(traverser._mergePointStack._mergePoints, Standard());
    TMergePointIterator endMp = end(traverser._mergePointStack._mergePoints, Standard());
    TMergePointIterator itMpLeft = endMp -1;
    // TODO(rmaerker): Check if binary search could be faster?

    // NOTE(rmaerker): The merge points are sorted in decreasing order from left to right.
    for (; itMpLeft != beginMp && *itMpLeft < static_cast<TSize>(contextBeginPosHost); --itMpLeft);
    TMergePointIterator itMp = itMpLeft;
    for (; itMp != beginMp && *itMp < *traverser._branchNodeIt; --itMp);
    ++itMp;  // Either it points to the first no

    TBranchNodeIterator itBp = traverser._branchNodeIt - 1;
    TBranchNodeIterator itBpBegin = traverser._branchNodeIt;
    for (; itBpBegin != begin(keys(variantData(container(traverser))), Rooted()) && static_cast<TPosition>(*itBpBegin) >= contextBeginPosHost; --itBpBegin);
    ++itBpBegin;

    TSize newOffset = 0;
    proxyId = bitScanForward(branchCoverage);
    //    traverser._activeBranchCoverage = branchCoverage;

    // Linear scan over merge and branch points to find the branch begin point which is furthest to the left.
    while(true)
    {
        // One of the iterators is at the end.
        if (itBp < itBpBegin || itMp > itMpLeft)
            break;

        // What happens if one is empty and the other not!
        if (*itMp > *itBp)
        {
            transform(tmp, traverser._mergePointStack._mergePointCoverage[itMp - beginMp], seenVariants,
                      FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
//            bitwiseAndNot(tmp, traverser._mergePointStack._mergePointCoverage[itMp - beginMp], seenVariants);
            if (!testAllZeros(tmp))  // Some of this variants are new.
            {
                transform(seenVariants, seenVariants, tmp, FunctorBitwiseOr());
                transform(tmp, tmp, branchCoverage, FunctorBitwiseAnd());
//                bitwiseOr(seenVariants, seenVariants, tmp);
//                bitwiseAnd(tmp, tmp, branchCoverage);
                if (!testAllZeros(tmp))  // There is a coverage containing this position.
                {
                    proxyId = bitScanForward(tmp);
                    newOffset = *traverser._branchNodeIt - *itMp;
                }
            }
            ++itMp;
        }
        if (*itBp >= *itMp)
        {
            transform(tmp, mappedCoverage(variantData(container(traverser)), position(itBp)), seenVariants,
                          FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
            if (!testAllZeros(tmp))  // Some of this variants are new.
            {
                transform(seenVariants, seenVariants, tmp, FunctorBitwiseOr());
                transform(tmp, tmp, branchCoverage, FunctorBitwiseAnd());
                if (!testAllZeros(tmp))  // There is a coverage containing this position.
                {
                    newOffset = (*traverser._branchNodeIt - *itBp) - 1;
                    proxyId = bitScanForward(tmp);
                }
            }
            --itBp;
        }
    }

    if (itMp > itMpLeft)
    {
        while (itBp >= itBpBegin)
        {
            transform(tmp, mappedCoverage(variantData(container(traverser)), position(itBp)), seenVariants,
                                      FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
            if (!testAllZeros(tmp))  // Some of this variants are new.
            {
                transform(seenVariants, seenVariants, tmp, FunctorBitwiseOr());
                transform(tmp, tmp, branchCoverage, FunctorBitwiseAnd());
                if (!testAllZeros(tmp))  // There is a coverage containing this position.
                {
                    newOffset = (*traverser._branchNodeIt - *itBp) - 1;
                    proxyId = bitScanForward(tmp);
                }
            }
            --itBp;
        }
    }
    if (itBp < itBpBegin)
    {
        while (itMp <= itMpLeft)
        {
            transform(tmp, traverser._mergePointStack._mergePointCoverage[itMp - beginMp], seenVariants,
                                  FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
            if (!testAllZeros(tmp))  // Some of this variants are new.
            {
                transform(seenVariants, seenVariants, tmp, FunctorBitwiseOr());
                transform(tmp, tmp, branchCoverage, FunctorBitwiseAnd());
                if (!testAllZeros(tmp))  // There is a coverage containing this position.
                {
                    proxyId = bitScanForward(tmp);
                    newOffset = *traverser._branchNodeIt - *itMp;
                }
            }
            ++itMp;
        }
    }

    return newOffset;
}

// ----------------------------------------------------------------------------
// Function _traverseBranchWithAlt()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPosition, typename TRequireFullContext,
          typename TExternal, typename TDelegate>
void
_traverseBranchWithAlt(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPosition, TRequireFullContext> > & traverser,
                       TExternal & externalAlg,
                       TDelegate & delegate)
{
    typedef JstTraverser<TContainer, TState, JstTraverserSpec<TContextPosition, TRequireFullContext> > TJstTraverser;

    typedef typename JournalData<TContainer>::Type TJournalSet;
    typedef typename Value<TJournalSet>::Type TJournalString;
    typedef typename Iterator<TJournalString, Standard>::Type TJournalStringIterator;
    typedef typename VariantData<TContainer>::Type TDeltaMap;
//    typedef typename MappedCoverage<TDeltaMap>::Type TBitVector;
    typedef typename MappedDelta<TDeltaMap>::Type TMappedDelta;
    typedef typename TDeltaMap::TDeltaStore_ TDeltaStore;
    typedef typename DeltaValue<TDeltaStore, DeltaType::DELTA_TYPE_INS>::Type TIns;
    typedef typename DeltaValue<TDeltaStore, DeltaType::DELTA_TYPE_INDEL>::Type TIndel;

//    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIterator;
    typedef typename Size<TContainer>::Type TSize;

    typedef typename TJstTraverser::TBranchStack TBranchStack;
    typedef typename Value<TBranchStack>::Type TBranchStackEntry;
//    typedef BranchStackNode<TJournalString, TBitVector, TBranchNodeIterator, TCallerState> TBranchStackNode;
//    typedef String<TBranchStackNode> TDeltaBranchStack;

    // At the begin determine the starting point of the branch.
    // This operation removes all ambiguous deltas. That is, if there was a deletion reaching into the current delta, and some sequences share both the prev del and the current delta.

//    TDeltaBranchStack branchStack;
//    resize(branchStack, 1, Exact());
    TBranchStackEntry & nextBranch = createInitialEntry(traverser._branchStack);

    // We start by initializing the branch coverage and the active branch coverage.
    nextBranch._branchCoverage = mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt));

    if (TRequireFullContext::VALUE)
        nextBranch._prefixOffset = _selectValidBeginAndProxy(nextBranch._branchProxyId, traverser,
                                                             _windowBeginPosition(traverser, StateTraverseMaster()),
                                                             nextBranch._branchCoverage);

#ifdef DEBUG_DATA_PARALLEL
        std::cerr << "Selected Proxy: " << nextBranch._branchProxyId << std::endl;
#endif

    // The coverage cannot be empty.
    SEQAN_ASSERT_GEQ(nextBranch._branchProxyId, 0u);
    SEQAN_ASSERT_LT(nextBranch._branchProxyId, journalSize(container(traverser)));

    TJournalString & proxySeq = getProxy(container(traverser), nextBranch._branchProxyId);
    TJournalStringIterator branchBeginIt;

    _mapHostToVirtual(nextBranch._proxyIter, proxySeq, variantData(container(traverser)), nextBranch._branchProxyId,
                      *traverser._branchNodeIt);  // Maybe this can be simplified.

    nextBranch._mappedHostPos = *traverser._branchNodeIt + 1;
    TSize contextSizeRight = windowSize(traverser);
    TMappedDelta varKey = mappedDelta(variantData(container(traverser)), position(traverser._branchNodeIt));
    switch(deltaType(varKey))
    {
        case DeltaType::DELTA_TYPE_DEL:
        {
            nextBranch._proxyEndPosDiff = deltaDel(variantData(container(traverser)), deltaPosition(varKey));
            if (--nextBranch._proxyEndPosDiff > 0)
            {
                nextBranch._mappedHostPos += nextBranch._proxyEndPosDiff;  // Moves right by the size of the deletion.
                // TODO(rmaerker): We can ompit this.
                transform(traverser._delCoverage, traverser._delCoverage, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)),
                          FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());  // We can set this first after the we searched through the original space.
//                bitwiseAndNot(traverser._delCoverage, traverser._delCoverage, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)));  // We can set this first after the we searched through the original space.
                // TODO(rmaerker): Optimize.
                push(traverser._mergePointStack, nextBranch._mappedHostPos, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)));
            }

            if (nextBranch._prefixOffset == 0)
            {
#ifdef DEBUG_DATA_PARALLEL
                std::cerr << "Points directly into deleted area." << std::endl;
#endif
                return;  // Points directly into deleted area!
            }
            --nextBranch._proxyIter;  // Now it points to the position before the deletion.
            --nextBranch._prefixOffset;
//            viewEnd += traverser._windowSize - 1;
            break;
        }
        case DeltaType::DELTA_TYPE_INS:
        {
//            viewEnd += (viewBegin._journalEntriesIterator->length + traverser._windowSize);  // Set one after the insertion.  // Requires to use also a variant with INS followed by DEL
            TIns & insVar = deltaIns(variantData(container(traverser)), deltaPosition(varKey));
            nextBranch._proxyEndPosDiff = -static_cast<int>(length(insVar));
            contextSizeRight += length(insVar);
//            ++mappedHostPos;
            break;
        }
        case DeltaType::DELTA_TYPE_INDEL:
        {
            TIndel & indel = deltaIndel(variantData(container(traverser)), deltaPosition(varKey));
            nextBranch._proxyEndPosDiff = indel.i1;
            nextBranch._proxyEndPosDiff -= static_cast<int>(length(indel.i2)) - 1;
            contextSizeRight += length(indel.i2);
            if (indel.i1 > 1)
            {
                nextBranch._mappedHostPos += indel.i1 - 1;  // Moves right by the size of the deletion.
                transform(traverser._delCoverage, traverser._delCoverage, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)),
                          FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());  // We can set this first after the we searched through the original space.
//                bitwiseAndNot(traverser._delCoverage, traverser._delCoverage, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)));  // We can set this first after the we searched through the original space.
                push(traverser._mergePointStack, nextBranch._mappedHostPos, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)));
            }
            break;
        }
    }

    nextBranch._externalState = traverser._lastMasterState;
    nextBranch._proxyEndPos = position(nextBranch._proxyIter) + contextSizeRight;

    _traverseBranch(traverser, externalAlg, delegate);
    while (pop(traverser._branchStack))
    {
        TBranchStackEntry & nextBranch = top(traverser._branchStack);
        // We first check if we need to update the current branch split.
        if (nextBranch._prefixOffset <= 0)  // The branch starts directly within the variant.
        {
            nextBranch._branchProxyId = bitScanForward(nextBranch._branchCoverage);
        }
        else
        {
            if (TRequireFullContext::VALUE)
            {
                int newOffset = _selectValidBeginAndProxy(nextBranch._branchProxyId, traverser,
                                                          _windowBeginPosition(traverser, StateTraverseMaster()),
                                                          nextBranch._branchCoverage);

                if (newOffset < nextBranch._prefixOffset)
                    nextBranch._prefixOffset = newOffset;
            }

            if (nextBranch._prefixOffset == 0 &&
                deltaType(mappedDelta(variantData(container(traverser)), position(traverser._branchNodeIt))) == DeltaType::DELTA_TYPE_DEL)
            {
                continue;
            }
        }

        SEQAN_ASSERT_GEQ(nextBranch._branchProxyId, 0u);
        SEQAN_ASSERT_LT(nextBranch._branchProxyId, journalSize(container(traverser)));

        TJournalStringIterator targetIt;
        targetIt._journalStringPtr = &getProxy(container(traverser), nextBranch._branchProxyId);
        _mapVirtualToVirtual(targetIt, nextBranch._proxyIter, traverser._branchNodeIt, variantData(container(traverser)),
                             nextBranch._branchProxyId);

        nextBranch._proxyIter = targetIt;  // NOTE(rmaerker): We could do a swap here, because we don't need the target iter no more.
        nextBranch._proxyEndPos = position(nextBranch._proxyIter) + contextSizeRight;

        setState(externalAlg, nextBranch._externalState);
        _traverseBranch(traverser, externalAlg, delegate);
    }
}

template <typename TContainer, typename TState, typename TExternal, typename TDelegate>
void _traverseBranchWithAlt(JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, False> > & traverser,
                            TExternal & externalAlg,
                            TDelegate & delegate)
{
    typedef JstTraverser<TContainer, TState, JstTraverserSpec<ContextPositionRight, False> > TJstTraverser;

    typedef typename JournalData<TContainer>::Type TJournalSet;
    typedef typename Value<TJournalSet>::Type TJournalString;
    typedef typename Iterator<TJournalString, Standard>::Type TJournalStringIterator;
    typedef typename VariantData<TContainer>::Type TDeltaMap;
//    typedef typename MappedCoverage<TDeltaMap>::Type TBitVector;
    typedef typename MappedDelta<TDeltaMap>::Type TMappedDelta;
    typedef typename TDeltaMap::TDeltaStore_ TDeltaStore;
    typedef typename DeltaValue<TDeltaStore, DeltaType::DELTA_TYPE_INS>::Type TIns;
    typedef typename DeltaValue<TDeltaStore, DeltaType::DELTA_TYPE_INDEL>::Type TIndel;

//    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIterator;
    typedef typename Size<TContainer>::Type TSize;

    typedef typename TJstTraverser::TBranchStack TBranchStack;
    typedef typename Value<TBranchStack>::Type TBranchStackEntry;
//    typedef BranchStackNode<TJournalString, TBitVector, TBranchNodeIterator, TCallerState> TBranchStackNode;
//    typedef String<TBranchStackNode> TDeltaBranchStack;

    // At the begin determine the starting point of the branch.
    // This operation removes all ambiguous deltas. That is, if there was a deletion reaching into the current delta, and some sequences share both the prev del and the current delta.

//    TDeltaBranchStack branchStack;
//    resize(branchStack, 1, Exact());
    TBranchStackEntry & nextBranch = createInitialEntry(traverser._branchStack);

    // Now how do we process here.
    // Unfortunately we have to check the deletions, because we cannot map one vp to another if they don't share the node.

    // Which is: a) prev node is directly before the current and they share all coverage.
    // Which is: b) There is a merge point directly ending on the current node and this one is deleted.
    nextBranch._branchCoverage = mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt));
    nextBranch._branchProxyId = bitScanForward(nextBranch._branchCoverage);
    nextBranch._proxyEndPosDiff = 0;

#ifdef DEBUG_DATA_PARALLEL
        std::cerr << "Selected Proxy: " << nextBranch._branchProxyId << std::endl;
#endif

    // Stop looking into the branch because it is invalid.
    if (nextBranch._branchProxyId >= journalSize(container(traverser)))
        return;

    // The coverage cannot be empty.
    SEQAN_ASSERT_GEQ(nextBranch._branchProxyId, 0u);
    SEQAN_ASSERT_LT(nextBranch._branchProxyId, journalSize(container(traverser)));

    TJournalString & proxySeq = getProxy(container(traverser), nextBranch._branchProxyId);
    TJournalStringIterator branchBeginIt;

    _mapHostToVirtual(nextBranch._proxyIter, proxySeq, variantData(container(traverser)), nextBranch._branchProxyId,
                      *traverser._branchNodeIt);

    // nextBranch._windowDeltaBeginIt points to first position within the branch or the first position after the deletion.
    nextBranch._mappedHostPos = *traverser._branchNodeIt + 1;
    TMappedDelta varKey = mappedDelta(variantData(container(traverser)), position(traverser._branchNodeIt));
    TSize contextSizeRight =  windowSize(traverser);
    switch(deltaType(varKey))
    {
        case DeltaType::DELTA_TYPE_DEL:
        {
            nextBranch._proxyEndPosDiff = deltaDel(variantData(container(traverser)), deltaPosition(varKey));

            // We only consider deletions bigger than 1.
            if (nextBranch._proxyEndPosDiff > 1)
            {
                nextBranch._mappedHostPos += nextBranch._proxyEndPosDiff - 1;  // Moves right by the size of the deletion.
                // Add the new merge point to the merge point stack.
                transform(traverser._delCoverage, traverser._delCoverage, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)),
                          FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
                push(traverser._mergePointStack, nextBranch._mappedHostPos, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)));
            }
            // We update the right size.
            --contextSizeRight;
            break;
        }
        case DeltaType::DELTA_TYPE_INS:
        {
            TIns & insVar = deltaIns(variantData(container(traverser)), deltaPosition(varKey));
            nextBranch._proxyEndPosDiff = -static_cast<int>(length(insVar));
            contextSizeRight += length(insVar);
            break;
        }
        case DeltaType::DELTA_TYPE_INDEL:
        {
            TIndel & indel = deltaIndel(variantData(container(traverser)), deltaPosition(varKey));
            nextBranch._proxyEndPosDiff = indel.i1;
            nextBranch._proxyEndPosDiff -= static_cast<int>(length(indel.i2)) - 1;
            contextSizeRight += length(indel.i2);
            if (indel.i1 > 1)
            {
                nextBranch._mappedHostPos += indel.i1 - 1;  // Moves right by the size of the deletion.
                transform(traverser._delCoverage, traverser._delCoverage, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)),
                                          FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
                push(traverser._mergePointStack, nextBranch._mappedHostPos, mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt)));
            }
            break;
        }
    }

    // Fill the state with the slected proxy until the end.
    nextBranch._externalState = traverser._lastMasterState;
    nextBranch._proxyEndPos  = position(nextBranch._proxyIter) + contextSizeRight;
    nextBranch._prefixOffset = windowSize(traverser) - 1; // We assume single base movement. -> This is a strong assumption maybe not always valid!

    _traverseBranch(traverser, externalAlg, delegate);
    while (pop(traverser._branchStack))
    {
        // Here we setup the new branch.
        TBranchStackEntry & nextBranch = top(traverser._branchStack);
        // Now we come back with the reduced branch coverage.
        SEQAN_ASSERT(!testAllZeros(nextBranch._branchCoverage));  // The coverage cannot be empty.
//        SEQAN_ASSERT(nextBranch._branchProxyId != bitScanForward(nextBranch._branchCoverage));  // The split coverage cannot be the same as before.

        nextBranch._branchProxyId = bitScanForward(nextBranch._branchCoverage);
        if (nextBranch._branchProxyId > journalSize(container(traverser)))  // No valid proxy can be found.
            continue;

        TJournalStringIterator targetIt;
        targetIt._journalStringPtr = &getProxy(container(traverser), nextBranch._branchProxyId);

        // It might be necessary to select from the host position instead of the virtual mapping.
        if (deltaType(varKey) == DeltaType::DELTA_TYPE_DEL)
            _mapHostToVirtual(targetIt, getProxy(container(traverser), nextBranch._branchProxyId),
                              variantData(container(traverser)), nextBranch._branchProxyId, *traverser._branchNodeIt);
        else
            _mapVirtualToVirtual(targetIt, nextBranch._proxyIter, traverser._branchNodeIt,
                                 variantData(container(traverser)), nextBranch._branchProxyId);

        // Now targetIt points to the position of
        nextBranch._proxyIter = targetIt;  // NOTE(rmaerker): We could do a swap here, because we don't need the target iter any more.
        nextBranch._proxyEndPos = position(nextBranch._proxyIter) + contextSizeRight;

        // We need to be careful, because, when the current variant is a deletion this might point directly into the deletion.
        setState(externalAlg, nextBranch._externalState);
        _traverseBranch(traverser, externalAlg, delegate);
    }
}

// ----------------------------------------------------------------------------
// Function _syncAndUpdateCoverage()
// ----------------------------------------------------------------------------

template <typename TJstTraverser>
inline bool
_syncAndUpdateCoverage(TJstTraverser & traverser,
                       StateTraverseMaster const & /*tag*/)
{
    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIterator;
    typedef typename Container<TJstTraverser>::Type TContainer;
    typedef typename Position<TContainer>::Type TPosition;
    typedef typename MakeSigned<TPosition>::Type TPos_;

    // At the current master position might be a deletion over and we merge the values.
    register unsigned oldLength = length(traverser._mergePointStack._mergePoints);
    bool needUpdate = _syncToMergePoint(traverser._delCoverage, traverser._mergePointStack,
                                        clippedWindowBeginPosition(traverser, StateTraverseMaster()),
                                        MergePointSyncResize()) != oldLength;

    // The new window begin position is after the left most node within the context.
    if (!atEnd(traverser._branchNodeInContextIt) &&
        _windowBeginPosition(traverser, StateTraverseMaster()) > static_cast<TPos_>(*traverser._branchNodeInContextIt))
    {
        // Set correct context branch node iterator.
         while (!atEnd(++traverser._branchNodeInContextIt) &&
                _windowBeginPosition(traverser, StateTraverseMaster()) > static_cast<TPos_>(*traverser._branchNodeInContextIt));

         // The context node must be smaller or equal the current branch node.
         SEQAN_ASSERT_LEQ(traverser._branchNodeInContextIt, traverser._branchNodeIt);

         // Now we update the current master coverage, while considering all deltas within the current context.
         traverser._activeMasterCoverage = traverser._delCoverage;
         for(TBranchNodeIterator it = traverser._branchNodeInContextIt; it != traverser._branchNodeIt; ++it)
             transform(traverser._activeMasterCoverage,
                       traverser._activeMasterCoverage, mappedCoverage(variantData(container(traverser)), position(it)),
                       FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
         return true;
    }

    if (needUpdate)  // Update the active master coverage with the delCoverage.
        transform(traverser._activeMasterCoverage, traverser._activeMasterCoverage, traverser._delCoverage, FunctorBitwiseAnd());
    return false;
}

template <typename TJstTraverser>
inline bool
_syncAndUpdateCoverage(TJstTraverser & traverser,
                       StateTraverseBranch const & /*tag*/)
{
    typedef typename TJstTraverser::TBranchNodeIterator TBranchNodeIterator;
    typedef typename Container<TJstTraverser>::Type TContainer;
    typedef typename Position<TContainer>::Type TPos;
    typedef typename MakeSigned<TPos>::Type TPos_;

    typedef typename TJstTraverser::TMergePointStore TMergePointStack;
    typedef typename TMergePointStack::TMergePoints TMergePoints;
    typedef typename Iterator<TMergePoints, Standard>::Type TMergePointIterator;

    typedef typename TJstTraverser::TBranchStack TBranchStack;
    typedef typename Value<TBranchStack>::Type TBranchStackEntry;


    // At the current branch position there might be a deletion and we merge the values.
    // register unsigned oldLength = length(traverser._mergePointStack._mergePoints);

    // We need the correct begin position within the master branch.
    // Now we assume the window can begin in any prefix that has a path that includes the current branch node.
    // Since, we already searched all previous branch nodes that include the current node on their path,
    // we need to find only those who come directly from the master branch.

    // To find only the valid coverage we move left through the previous nodes and merge points that can affect the
    // current prefix.

    // Simple case, the current window begin maps directly onto the current delta.
    TBranchStackEntry & branchStackEntry = top(traverser._branchStack);
    traverser._activeBranchCoverage = branchStackEntry._branchCoverage;
    if (_windowBeginPosition(traverser, StateTraverseBranch()) >= static_cast<TPos_>(position(branchStackEntry._proxyIter)))
        return false;  // Nothing to do, we can keep the current coverage.

    // Harder case, we need to determine all sequences that had not been destroyed by the previous operations.
    // But we don't know, from which path the current proxy comes. -> Can be the correct one or not.
    // But we know:
     unsigned sDelta = _windowEndPosition(traverser, StateTraverseBranch()) - position(branchStackEntry._proxyIter);
     SEQAN_ASSERT_LEQ(sDelta, windowSize(traverser) -1);
     unsigned pDelta = windowSize(traverser) - 1 - sDelta;

     // In some rare cases, where there is a delta direct at beginning, it might be possible that we
     // reach over the begin position of the delta.
     unsigned beginHostPos = 0;
     if (pDelta <= *traverser._branchNodeIt)
         beginHostPos = *traverser._branchNodeIt - pDelta;

     SEQAN_ASSERT_LEQ(beginHostPos, *traverser._branchNodeIt);

     // Considering merge points.
     // Only the intersection of *mp and cov is valid for all: beginHostPos <= *mp <= *branchNodeIt;
     TMergePointIterator it = end(traverser._mergePointStack._mergePoints) - 1;
     TMergePointIterator itBegin = begin(traverser._mergePointStack._mergePoints);

     for(;it != itBegin; --it)
         if (*it >= beginHostPos && *it < *traverser._branchNodeIt)
             transform(traverser._activeBranchCoverage, traverser._activeBranchCoverage,
                       traverser._mergePointStack._mergePointCoverage[it - itBegin],
                       FunctorBitwiseAnd());

     // Considering previous deltas.
     // For all bp: beginHostPos <= ~*bp < *branchNodeIt
     TBranchNodeIterator tmpIt = traverser._branchNodeIt;
     TBranchNodeIterator tmpItBegin = begin(variantData(container(traverser)), Rooted());
     for (; tmpIt != tmpItBegin && *tmpIt == *traverser._branchNodeIt; --tmpIt);  // Only nodes that are not equal to the current.

     for (; tmpIt != tmpItBegin && *tmpIt >= beginHostPos; --tmpIt)
         transform(traverser._activeBranchCoverage, traverser._activeBranchCoverage,
                   mappedCoverage(variantData(container(traverser)), position(tmpIt)),
                   FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());

     if (tmpIt == tmpItBegin && *tmpIt >= beginHostPos  && *tmpIt < *traverser._branchNodeIt)
         transform(traverser._activeBranchCoverage, traverser._activeBranchCoverage,
                   mappedCoverage(variantData(container(traverser)), position(tmpIt)),
                   FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
    return false;
}

// ----------------------------------------------------------------------------
// Function _execTraversal()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPosition, typename TRequireFullContext,
          typename TExternal, typename TDelegate>
inline void
_execTraversal(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPosition, TRequireFullContext> > & traverser,
               TExternal & externalAlg,
               TDelegate & delegate)
{
    typedef typename VariantData<TContainer>::Type TDeltaMap;
    typedef typename MappedCoverage<TDeltaMap>::Type TBitVector;

#ifdef PROFILE_DATA_PARALLEL_INTERN
    String<double> timeTable;
    resize(timeTable, 3, 0.0, Exact());
    double timeAll = sysTime();
#endif
//    unsigned deltaCounter = 0;
//    TMasterBranchIterator masterBranchSegmentEnd = begin(host(container(haystack(finder)))) +
//        *finder._branchNodeIt - length(container(finder._needleIt)) + 1;
#ifdef PROFILE_DATA_PARALLEL_INTERN
    unsigned counter = 0;
    unsigned currentPercentage = 0;
    unsigned fivePercentInterval = ((traverser._branchNodeItEnd - traverser._branchNodeIt) * 5) / 100;
    std::cerr << currentPercentage << "% " << std::flush;
#endif //PROFILE_DATA_PARALLEL

    traverser._lastMasterState = getState(externalAlg);

        // Loop over the branch nodes.
    while (traverser._branchNodeIt != traverser._branchNodeItEnd)
    {
#ifdef PROFILE_DATA_PARALLEL_INTERN
        double timeMaster = sysTime();
#endif

#ifdef DEBUG_DATA_PARALLEL
    std::cerr << "\n" << "#####################" << std::endl;
    std::cerr << "Search Master Segment: " << position(windowBegin(traverser, StateTraverseMaster())) << " - " << *traverser._branchNodeIt << std::endl;
    std::cerr << "Breakpoint: " << *traverser._branchNodeIt << std::endl;
    std::cerr << "Coverage: " << traverser._activeMasterCoverage << std::endl;
#endif
        if (isMaster(traverser))
        {
            setState(externalAlg, traverser._lastMasterState);  // Reactivate the last state.
            // Search along the master strand.
            while (_windowEndPosition(traverser, StateTraverseMaster()) < *traverser._branchNodeIt)
            {
                traverser._masterIt += deliverContext(externalAlg, delegate, traverser, StateTraverseMaster());
//                refresh(traverser, externalAlg, delegate, StateTraverseMaster());
            }
        }
#ifdef PROFILE_DATA_PARALLEL_INTERN
        timeTable[0] += sysTime() - timeMaster;
#endif

        traverser._traversalState = JST_TRAVERSAL_STATE_BRANCH;

        // Processing the current node.
        unsigned branchPosition = *traverser._branchNodeIt;

#ifdef DEBUG_DATA_PARALLEL
        std::cerr << "#####################" << std::endl;
        std::cerr << "Search Branch Segment: " << std::endl;
        std::cerr << "Master Branch Coverage: " << traverser._activeMasterCoverage << std::endl;
#endif

#ifdef PROFILE_DATA_PARALLEL_INTERN
        double timeBranchAll = sysTime();
#endif
        _syncAndUpdateCoverage(traverser, StateTraverseMaster());
        traverser._lastMasterState = getState(externalAlg);  // Keep the last active caller state.

        // Search all haplotypes with the alternative allel at this position.
        while(traverser._branchNodeIt != traverser._branchNodeItEnd && *traverser._branchNodeIt == branchPosition)
        {
#ifdef DEBUG_DATA_PARALLEL
            std::cerr << "Coverage: " << traverser._activeBranchCoverage << std::endl;
#endif
            TBitVector& mappedCov = mappedCoverage(variantData(container(traverser)), position(traverser._branchNodeIt));
            if (!testAllZeros(mappedCov))
            {
                // We know search in the delta until we have reached the end position of this delta: x, if INS/SNP or x-1 if DEL
                // What if multiple deletions at the same position?
#ifdef PROFILE_DATA_PARALLEL_INTERN
                double timeBranch1 = sysTime();
#endif
                _traverseBranchWithAlt(traverser, externalAlg, delegate);
#ifdef PROFILE_DATA_PARALLEL_INTERN
                timeTable[1] += sysTime() - timeBranch1;
#endif
                // Remove the coverage from the current delta from the active master coverage.
                transform(traverser._activeMasterCoverage, traverser._activeMasterCoverage, mappedCov, FunctorNested<FunctorBitwiseAnd, FunctorIdentity, FunctorBitwiseNot>());
            }
            // We increase the delta
            ++traverser._branchNodeIt;

#ifdef PROFILE_DATA_PARALLEL_INTERN
            if (++counter == fivePercentInterval)
            {
                currentPercentage += 5;
                std::cerr << currentPercentage << "% " << std::flush;
                counter = 0;
            }
#endif //PROFILE_DATA_PARALLEL
        }
        traverser._traversalState = JST_TRAVERSAL_STATE_MASTER;
#ifdef PROFILE_DATA_PARALLEL_INTERN
        timeTable[2] += sysTime() - timeBranchAll;
#endif
    }
#ifdef PROFILE_DATA_PARALLEL_INTERN
    double timeMaster = sysTime();
#endif
    traverser._traversalState = JST_TRAVERSAL_STATE_MASTER;
    setState(externalAlg, traverser._lastMasterState);  // Reactivate the last state.

    // Set end of segment to next breakpoint.
#ifdef DEBUG_DATA_PARALLEL
    std::cerr << "#####################" << std::endl;
    std::cerr << "Search Master Segment: " << position(windowBegin(traverser, StateTraverseMaster())) << " - " << position(traverser._masterItEnd) << std::endl;
#endif

    while (windowEnd(traverser, StateTraverseMaster()) < traverser._masterItEnd)
    {
        traverser._masterIt += deliverContext(externalAlg, delegate, traverser, StateTraverseMaster());
//        refresh(traverser, externalAlg, delegate, StateTraverseMaster());
#ifdef DEBUG_DATA_PARALLEL
    std::cerr << "--- position: " << position(windowBegin(traverser, StateTraverseMaster())) << std::endl;
#endif
    }
    // Synchronize to the merge points in the end.
    _syncToMergePoint(traverser._activeMasterCoverage, traverser._mergePointStack,
                      position(windowBegin(traverser, StateTraverseMaster())),
                      MergePointSyncResize());

#ifdef PROFILE_DATA_PARALLEL_INTERN
    timeTable[0] += sysTime() - timeMaster;
#endif

#ifdef PROFILE_DATA_PARALLEL_INTERN
    std::cerr <<  std::endl;
    std::cerr << "Time Master: " << timeTable[0] << " s." << std::endl;
    std::cerr << "Time Branch iterate: " << timeTable[1] << " s." << std::endl;
    std::cerr << "Time Branch all: " << timeTable[2] << " s." << std::endl;
    std::cerr << "Time total: " << sysTime() - timeAll << " s." <<std::endl;
#endif // PROFILE_DATA_PARALLEL
}

// ----------------------------------------------------------------------------
// Function _initSegment()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPosition, typename TRequireFullContext,
          typename TDeltaNodeIterator, typename THostSegmentBegin, typename THostSegmentEnd>
inline void
_initSegment(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPosition, TRequireFullContext> > & traverser,
             TDeltaNodeIterator const & nodeItBegin,
             TDeltaNodeIterator const & nodeItEnd,
             THostSegmentBegin const & hostSegmentBeginPosition,
             THostSegmentEnd const & hostSegmentEndPosition)
{
    traverser._masterIt = begin(host(container(traverser)), Rooted()) + hostSegmentBeginPosition;
    if (IsSameType<TContextPosition, ContextPositionRight>::VALUE && TRequireFullContext::VALUE)
        traverser._masterIt +=  (windowSize(traverser) - 1);
    traverser._masterItEnd = begin(host(container(traverser)), Rooted()) + hostSegmentEndPosition;
    traverser._branchNodeInContextIt = traverser._branchNodeIt = nodeItBegin;
    traverser._branchNodeItEnd = nodeItEnd;
    _globalInit(traverser);
}

// ----------------------------------------------------------------------------
// Function init()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TContextPosition, typename TRequireFullContext>
inline void
init(JstTraverser<TContainer, TState, JstTraverserSpec<TContextPosition, TRequireFullContext> > & traverser,
     TContainer & obj)
{
    setContainer(traverser, obj);
    _initSegment(traverser, begin(variantData(obj), Rooted()), end(variantData(obj), Rooted()), 0, length(host(obj)));
//    else
//    {
//        traverser._masterIt = begin(host(container(traverser)), Rooted());
//        if (IsSameType<TContextBegin, ContextBeginRight>::VALUE)
//            traverser._masterIt +=  (windowSize(traverser) - 1);
//        traverser._masterItEnd = end(host(container(traverser)), Rooted());
//    }
}

// ----------------------------------------------------------------------------
// Function traverse()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec, typename TOperator, typename TDelegate>
inline void
traverse(JstTraverser<TContainer, TState, TSpec> & traverser,
         TOperator & traversalCaller,
         TDelegate & delegate)
{
    _execTraversal(traverser, traversalCaller, delegate);
}

// ----------------------------------------------------------------------------
// Function setContainer()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline void
setContainer(JstTraverser<TContainer, TState, TSpec> & traverser,
             TContainer & container)
{
    traverser._haystackPtr = &container;
}

// ----------------------------------------------------------------------------
// Function setWindowSize()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec, typename TSize>
inline void
setWindowSize(JstTraverser<TContainer, TState, TSpec> & traverser,
             TSize const & newWindowSize)
{
    traverser._windowSize = newWindowSize;
}

// ----------------------------------------------------------------------------
// Function windowSize()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline typename Size<JstTraverser<TContainer, TState, TSpec> >::Type
windowSize(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    return traverser._windowSize;
}

// ----------------------------------------------------------------------------
// Function container()
// ----------------------------------------------------------------------------

template <typename TContainer, typename TState, typename TSpec>
inline typename Container<JstTraverser<TContainer, TState, TSpec> >::Type &
container(JstTraverser<TContainer, TState, TSpec> & traverser)
{
    return *traverser._haystackPtr;
}

template <typename TContainer, typename TState, typename TSpec>
inline typename Container<JstTraverser<TContainer, TState, TSpec> const>::Type &
container(JstTraverser<TContainer, TState, TSpec> const & traverser)
{
    return *traverser._haystackPtr;
}

}  // namespace seqan

#endif  // EXTRAS_INCLUDE_SEQAN_DATA_PARALLEL_DATA_PARALLEL_TRAVERSAL_H_
